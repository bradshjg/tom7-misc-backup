
(* Filterhero is a version of Renderhero that renders a MIDI to a
   bitmap and then can apply arbitrary functions to it. This is much
   more expensive approach than generating and flattening the
   samples as we go, which is why it is a separate version.
*)

structure FilterHero =
struct
  exception Hero of string
  structure MT = MersenneTwister

  val outputp = Params.param "" (SOME("-o", "Set the output file.")) "output"
  val gainp = Params.param "1.0" (SOME("-gain", "Gain multiplier.")) "gain"
  (* XX Note that when using this arg, it will often fail because it can't be done exactly.
     The way to fix this is to increase the sample rate -- could maybe do this for you.
     There's a significant performance penalty to computing more samples. Maybe also would
     need to downsample if using sample rates in the hundreds of kHz. *)
  val exactp = Params.flag false
      (SOME("-exact", "Require ticks to fall exactly on sample boundaries.")) "exact"

  val ratep = Params.param "44100" (SOME("-rate", "Sample rate in Hz. You may need " ^
                                         "to increase this if -exactp is used.")) "rate"

  fun getoutput song =
      case !outputp of
          "" => let val (base, ext) = FSUtil.splitext song
                in base ^ ".wav"
                end
        | s => s

  fun eprint s = TextIO.output(TextIO.stdErr, s ^ "\n")
  val itos = Int.toString
  fun rtos r = Real.fmt (StringCvt.FIX (SOME 3)) r

  val TWOPI = Math.pi * 2.0

  (* These are MIDI-level constants *)
  val NNOTES = 128
  val NCHANNELS = 16

  datatype inst =
      INST_NONE
    | INST_SQUARE
    | INST_SAW
    | INST_NOISE
    | INST_SINE

  (* no cur_freq -- use pitch to generate *)
  (* How loud is this note right now? 0 means off *)
  val cur_vol = Array.array(NCHANNELS * NNOTES, 0)
  val cur_inst = Array.array(NCHANNELS, INST_NONE)
  (* Number of notes on in the channel. Used to
     skip blank channels during mixing. *)
  val num_on = Array.array(NCHANNELS, 0)

  (* cur_val and leftover unused *)
  val samples = Array.array(NCHANNELS * NNOTES, 0)

  (* Not sure this is right for negative inputs.
     Definitely has precision loss in some situations. *)
  fun fmod (x, y) =
    let val rem = Real.realMod (x / y)
    in rem * y
    end

  (* Dummy event, used for bars and stuff *)
  val DUMMY = MIDI.META (MIDI.PROP "dummy")

  datatype bar =
      Measure
    | Beat
    | Timesig of int * int

  (* These are used to distinguish tracks, and then events, by
     the high-level purpose. *)
  datatype label =
      Music of inst
    | Control
    | Bar of bar

  fun setnote (ch, note, vol) =
    let
        val idx = ch * NNOTES + note
        val old = Array.sub (cur_vol, idx)
    in
        Array.update (cur_vol, idx, vol);
        (* If turning the note on or off, adjust the count. *)
        case (old, vol) of
            (0, 0) => ()
          | (0, n) => Array.update (num_on, ch, Array.sub (num_on, ch) + 1)
          | (n, 0) => Array.update (num_on, ch, Array.sub (num_on, ch) - 1)
          | (n, m) => ()
    end

  fun noteon (ch, note, vol, inst) =
    let in
        (* This probably never comes up because MIDI files use one
           instrument per track: MIDInverse assumes this, and since
           we label instruments by the track name, there's no possibility
           in the current approach. But to be fully general, this is wrong. *)
        Array.update (cur_inst, ch, inst);
        setnote (ch, note, vol)
    end

  fun noteoff (ch, note) = setnote (ch, note, 0)

  (* The crux of this fork is that "time" proceeds only through manual
     intervention. Each tick is a sample.

     Note: Assumes this doesn't overflow, which is probably safe for
     real music. *)
  val HAMMERSPEED = 1
  local val gt = ref 0
  in
    fun getticks () = !gt
    fun maketick () = gt := !gt + HAMMERSPEED
  end

  fun div_exact (m : IntInf.int, n) =
    let
        val q = m div n
    in
        if n * q <> m
        then raise Hero (IntInf.toString m ^ " div " ^
                         IntInf.toString n ^ " cannot be represented exactly")
        else q
    end

  fun div_exacti (m, n) = IntInf.toInt (div_exact (IntInf.fromInt m, IntInf.fromInt n))

  infix div_exact div_exacti

  (* GrowArray for the bitmaps. *)
  structure GA = GrowMonoArrayFn_Safe(structure A = Word8Array
                                      val default = 0w0 : Word8.word)
  (* One bitmap for each channel. Since it is very common for a channel
     to be completely unused, these are lazily allocated. A value of
     NONE can be treated as being all 0. Each array is a flattened
     rectangle in time-major order; the pixel (t, n) where t is a
     time and n is a note is located at t * 128 + n. *)
  val bitmaps =
    Array.array (NCHANNELS, NONE : GA.growarray option)

  fun mixaudio (gain, rate) =
    let
      val mt = MT.initstring "filterhero"

      (* These should all be the same. *)
      val nsamples = ref 0
      (* Just the nonempty tracks. *)
      val tracks = ref nil
      val () = Util.for 0 (NCHANNELS - 1)
        (fn ch =>
         case Array.sub (bitmaps, ch) of
           NONE => ()
         | SOME a =>
             let in
               tracks := (ch, a) :: !tracks;
               nsamples := GA.length a div NNOTES
             end)
      val nsamples = !nsamples
      val tracks = rev (!tracks)
      val samples = Array.array (nsamples, 0.0)

      (* Lay down tracks sequentially, mixing in place. *)
      fun onechannel (ch, arr) =
        let
          (* XXX Sloppy: We just assume this array still has the
             instrument for the channel. Since the bitmap process
             is lossy (pixels can't store instrument info), we
             might as well just say that each track has exactly
             one instrument, and then pass them here at mixing
             time, which will do the same thing as this hack: *)
          val inst = Array.sub(cur_inst, ch)
        in
          Util.for 0 (nsamples - 1)
          (fn s =>
           let
             (* Get existing magnitude to mix in *)
             val mag = ref (Array.sub (samples, s))
           in
             Util.for 0 (NNOTES - 1)
             (fn n =>
              case GA.sub arr (s * NNOTES + n) of
                  0w0 => ()
                | vel =>
              let
                 (* The length of one cycle in samples, for
                    this note's frequency. *)
                 val freq = MIDI.pitchof n
                 val cycle = rate / freq
                 val pos = fmod(real s, cycle)

                 val rvel = real (Word8.toInt vel) / 127.0

                 val impulse : real =
                   case inst of
                     INST_NONE => 0.0
                   | INST_NOISE =>
                      let val r = Real.fromLargeInt
                          (Word32.toLargeIntX (MT.rand32 mt))
                          (* note that for the most negative integer, the
                             fraction will actually be slightly larger than 1,
                             but default magnitude for noise is 0.5, so the final
                             value will be in range. *)
                          val denom = Real.fromLargeInt
                              (Word32.toLargeIntX 0wx7FFFFFFF)
                      in
                        0.5 * r / denom
                      end

                   | INST_SQUARE => if pos * 2.0 > cycle then rvel else ~rvel
                   | INST_SAW => rvel * ((pos / cycle) * 2.0 - 1.0)
                   | INST_SINE => rvel * Math.sin((pos / cycle) * TWOPI)
              in
                mag := !mag + impulse * gain
              end);
             Array.update (samples, s, !mag)
           end)
        end
    in
      app onechannel tracks;
      samples
    end

  (* Get the "color" of the pixel, which is the volume of the corresponding
     note at that time. Should be in [0, 127]. *)
  fun getpixel (vol, ch, note) : Word8.word =
    case Array.sub (cur_vol, ch * NNOTES + note) of
      0 => 0w0
    | vel =>
        if vel >= 128 orelse vel < 0
        then raise Hero ("Illegal velocity " ^ itos vel)
        else Word8.fromInt vel

  (* Get the bitmap for this channel, or create a new one if this is the
     first time we have nonzero pixels to write. In either case, it
     will contain data up to (but not including) time (t + n). *)
  fun getormakearray (t, ch, n) =
    case Array.sub (bitmaps, ch) of
      SOME a => a
    | NONE =>
        let val a = GA.init (t + n + 1)
        in
          (* initialize zero up to this point *)
          Util.for 0 (t + n - 1)
          (fn tt =>
           Util.for 0 (NNOTES - 1)
           (fn nn =>
            GA.append a 0w0));
          Array.update (bitmaps, ch, SOME a);
          a
        end

  (* generate n samples and write them to the end of the
     rendered bitmaps. Assumes we are at time t. *)
  fun mixpixels (t, rate : real, n) =
    Util.for 0 (n - 1)
    (fn nn =>
     Util.for 0 (NCHANNELS - 1)
     (fn ch =>
      if Array.sub (num_on, ch) = 0
      then ()
      else
      let val arr = getormakearray (t, ch, nn)
      in
        Util.for 0 (NNOTES - 1)
        (fn note =>
         GA.append arr (getpixel (cur_vol, ch, note)))
      end))

  (* Given a MIDI tempo in microseconds-per-quarter note
     (which is what the TEMPO event carries),
     compute the number of samples per MIDI tick,
     and insist that this can be represented exactly
     as an integer. *)
  fun spt_from_upq (divi, upq) =
    let
        val () = print ("Microseconds per quarternote: " ^
                        Int.toString upq ^ "\n")
        val rate = Params.asint 44100 ratep

        (* microseconds per tick  NO *)
        (* val uspt = IntInf.fromInt upq * IntInf.fromInt divi *)

        (* samples per tick is

           usec per tick      samples per usec
           (upq / divi)    *  (fps / 1000000)

           *)
        val fpq =
            if !exactp
            then (IntInf.fromInt rate * IntInf.fromInt upq) div_exact
                (IntInf.fromInt 1000000 * IntInf.fromInt divi)
            else IntInf.fromInt (Real.round ((real rate * real upq) /
                                             (1000000.0 * real divi)))
    in
        print ("Samples per tick is now: " ^ IntInf.toString fpq ^ "\n");
        IntInf.toInt fpq
    end

  fun loopplay (_, (_, _, _, nil)) = print "SONG END.\n"
    | loopplay (t, (divi, lt, spt, track)) =
      let
          val now = getticks ()

          (* hack attack *)
          val spt = ref spt
          (* val () = print ("Lt: " ^ itos lt ^ " Now: " ^ itos now ^ "\n") *)


          (* Last time we were here it was time 'lt', and now it is 'now'. The
             events in the track are measured as deltas from lt. We
             want to play any event that was late (or on time): that
             occurred between lt and now. This is the case when the delta time
             is less than now - lt.

             There may be many such events, each measured as a delta from the
             previous one. For example, we might have

                            now = 17
               1 1  2   5   |
               -A-B- -C- - -v- -D
              . : . : . : . : . :
              ^
              |
              lt = 10

              We want to emit events A-C, and reduce the delta time on D to 2,
              then continue with lt=now. To do so, we call the period of time
              left to emit the 'gap'; this begins as now - lt = 7.

              We then emit any events with delta time less than the gap,
              reducing the gap as we do so. (We can think of this as also
              updating lt, but there is no reason for that.)

              When the gap is too small to include the next event, we reduce
              its delta time by the remaining gap amount. *)
          fun nowevents _ nil = nil (* song will end on next trip *)

            | nowevents gap ((dt, (label, evt)) :: rest) =
              if dt <= gap
              then
                let in
                  (case label of
                     Music inst =>
                       (case evt of
                          MIDI.NOTEON(ch, note, 0) => noteoff (ch, note)
                        | MIDI.NOTEON(ch, note, vel) => (print ".";
                                                         noteon (ch, note, vel, inst))
                        | MIDI.NOTEOFF(ch, note, _) => noteoff (ch, note)
                        | _ => eprint ("unknown music event: " ^ MIDI.etos evt))
                   | Control =>
                       (case evt of
                            (* http://jedi.ks.uiuc.edu/~johns/links/music/midifile.html *)
                            MIDI.META (MIDI.TEMPO n) =>
                                let in
                                    eprint ("TEMPO " ^ itos n);
                                    spt := spt_from_upq (divi, n)
                                end

                          | MIDI.META (MIDI.TIME (n, d, cpc, bb)) =>
                                eprint ("myTIME " ^ itos n ^ "/" ^ itos (Util.pow 2 d) ^
                                        "  @ " ^ itos cpc ^ " bb: " ^ itos bb)
                          | _ => eprint ("unknown ctrl event: " ^ MIDI.etos evt))
                   | Bar _ => () (* Does nothing, but could draw. *)
                            );

                  nowevents (gap - dt) rest
                end
              (* the event is not ready yet. it must be measured as a
                 delta from 'now' *)
              else (dt - gap, (label, evt)) :: rest

          val track = nowevents (now - lt) track
      in
          loop (t, (divi, now, !spt, track))
      end

  and loop (t, arg as (_, _, spt, _)) =
      let
          val rate = Params.asint 44100 ratep
          (* render spt (samples-per-tick) samples *)
          val n = spt
      in
          mixpixels (t, real rate, n);

          (* advance time; continue *)
          maketick ();
          loopplay (t + n, arg)
      end

  (* In an already-labeled track set, add measure marker events.
     This is not currently used for generating the wav, but could be useful in
     generating a graphic. *)
  fun add_measures (divi, t) =
    let
      (* Read through tracks, looking for TIME events in
         Control tracks.
         Each TIME event starts a measure, naturally.
         We need to find one at 0 time, otherwise this is
         impossible:
         *)
      fun getstarttime nil = raise Hero "(TIME) no events?"
        | getstarttime ((0,
                         (_,
                          MIDI.META (MIDI.TIME (n, d, cpc, bb)))) :: rest) =
          ((n, d, cpc, bb), rest)
        | getstarttime ((0, evt) :: t) =
          let val (x, rest) = getstarttime t
          in (x, (0, evt) :: rest)
          end
        | getstarttime (_ :: t) = raise Hero "(TIME) no 0 time events"

      val ((n, d, cpc, bb), rest) = getstarttime t

      val () = if bb <> 8
               then raise Hero "The MIDI file may not redefine 32nd notes!"
               else ()

      (* We ignore the clocksperclick, which gives the metronome rate. Even
         though this is ostensibly what we want, the values are fairly
         unpredictable. *)

      (* First we determine the length of a measure in midi ticks.

         The midi division tells us how many clicks there are in a quarter note.
         The denominator tells us how many beats there are per measure. So we can
         first compute the number of midi ticks in a beat: *)
      val beat =
        (* in general  divi/(2^(d-2)), but avoid using fractions
           by special casing the d < 2 *)
        case d of
          (* n/1 time *)
          0 => divi * 4
          (* n/2 time: half notes *)
        | 1 => divi * 2
          (* n/4 time, n/8 time, etc. *)
        | _ => divi div (Util.pow 2 (d - 2))

      val () = eprint ("The beat value is: " ^ Int.toString beat)

      val measure = n * beat
      val () = eprint ("And the measure value is: " ^ Int.toString measure)

      (* XXX would be better if we could also additionally label the beats when
         using a complex time, like for sensations: 5,5,6,5. Instead we just
         put one minor bar for each beat. *)

      (* For now the beat list is always [1, 1, 1, 1 ...] so that we put
         a minor bar on every beat. *)
      val beatlist = List.tabulate (n, fn _ => 1)

      (* FIXME test *)
      (* val beatlist = [5, 5, 6, 5] *)

      (* number of ticks at which we place minor bars; at end we place a major one *)
      val ticklist = map (fn b => b * beat) beatlist

      fun ibars tl nil = nil (* XXX probably should finish out
                                the measure, draw end bar. *)

        | ibars (ticksleft :: rtl) ((dt, evt) :: rest) =
        (* Which comes first, the next event or our next bar? *)
        if dt <= ticksleft
        then
          (case evt of
             (* Time change event coming up! *)
             (Control, MIDI.META (MIDI.TIME _)) =>
             (* emit dummy event so that we can always start time changes with dt 0 *)
             (dt, (Control, DUMMY)) :: add_measures (divi, (0, evt) :: rest)

           | _ => (dt, evt) :: ibars (ticksleft - dt :: rtl) rest)
        else
          (* if we exhausted the list, then there is a major (measure) bar here. *)
          (case rtl of
             nil => (ticksleft, (Bar Measure, DUMMY)) ::
               ibars ticklist ((dt - ticksleft, evt) :: rest)
           | _   => (ticksleft, (Bar Beat, DUMMY)) ::
               ibars rtl      ((dt - ticksleft, evt) :: rest))

        | ibars nil _ = raise Hero "tickslist never nil" (* 0/4 time?? *)

    in
      (0, (Control, MIDI.META (MIDI.TIME (n, d, cpc, bb)))) ::
      (0, (Bar (Timesig (n, Util.pow 2 d)), DUMMY)) ::
      ibars ticklist rest
    end

  (* Delay a track by 2 * divi delta ticks. *)
  fun delay (divi, t) = (2 * divi, (Control, DUMMY)) :: t

  (* Label all of the tracks as Music or Control, based in their names. *)
  fun label tracks =
    let
        (* get the name of a track *)
        fun findname nil = NONE
          | findname ((_, MIDI.META (MIDI.NAME s)) :: _) = SOME s
          | findname (_ :: rest) = findname rest

        fun getlabel "" = NONE
          | getlabel name =
            (case CharVector.sub(name, 0) of
                 #"+" => SOME (case CharVector.sub (name, 1) of
                                   #"Q" => Music INST_SQUARE
                                 | #"W" => Music INST_SAW
                                 | #"N" => Music INST_NOISE
                                 | #"S" => Music INST_SINE
                                 | _ => raise Hero ("If the name starts with +, expected " ^
                                                    "one of the instrument types I know: " ^
                                                    name))
               | _ => NONE)

        fun onetrack tr =
            case findname tr of
                NONE => SOME (Control, tr)
              | SOME name =>
                    (case getlabel name of
                         NONE => SOME (Control, tr)
                       | SOME ty => SOME (ty, tr))
    in
        List.mapPartial onetrack tracks
    end

  fun gaussian_blur NONE = ()
    | gaussian_blur (SOME a) =
    let
      val mix = 0.05
      val ommix = 1.0 - mix
      val nsamples = GA.length a div NNOTES
      val () = if GA.length a mod NNOTES = 0
               then ()
               else raise Hero "bitmap is not NCHANNELS tall??"
      fun get (s, n) = real (Word8.toInt (GA.sub a (s * NNOTES + n))) / 127.0
      fun set (s, n) r =
        let val w = Word8.fromInt (Real.trunc (r * 127.0))
        in GA.update a (s * NNOTES + n) w
        end
      fun gb () =
        Util.for 1 (nsamples - 2)
        (fn s =>
         Util.for 1 (NNOTES - 2)
         (fn n =>
          let
            val this = get (s, n)
            val up = get (s, n - 1)
            val down = get (s, n + 1)
            val left = get (s - 1, n)
            val right = get (s + 1, n)
          in
            set (s, n - 1) (ommix * up + mix * this);
            set (s, n + 1) (ommix * down + mix * this);
            set (s - 1, n) (ommix * left + mix * this);
            set (s + 1, n) (ommix * right + mix * this);
            set (s, n) ((1.0 - 4.0 * mix) * this +
                        mix * up + mix * down + mix * left + mix * right)
          end))
    in
      Util.for 0 4
      (fn i =>
       let in
         eprint ("Gaussian blur pass " ^ itos i ^ "...");
         gb ()
       end)
    end


  fun render song =
    let
      val OUTPUT = getoutput song
      val r = (Reader.fromfile song) handle _ =>
        raise Hero ("couldn't read " ^ song)
      val m as (ty, divi, thetracks) = MIDI.readmidi r
          handle e as (MIDI.MIDI s) =>
              raise Hero ("Couldn't read MIDI file: " ^ s ^ "\n")
      val _ = ty = 1 orelse raise Hero ("MIDI file must be type 1 (got type " ^
                                        itos ty ^ ")")
      val () = eprint ("MIDI division is " ^ itos divi ^ "\n")
      val _ = divi > 0 orelse raise Hero "Division must be in PPQN form!\n"

      (* Label them with Music/Control *)
      val tracks = label thetracks
      (* Merge that down onto a single track *)
      val track = MIDI.mergea tracks

      val track = add_measures (divi, track)

      (* This used to add a single event to delay the start of the song, but
         that is not desirable here. *)
      (* val tracks = delay tracks *)

      (* Start with the play loop, because we want to begin at time 0, sample 0 *)
      (* need to start with tempo 120 = 0x07a120 ms per tick *)
      val () = loopplay (0, (divi, getticks (), spt_from_upq (divi, 0x07a120), track))

      val () =
        Util.for 0 (NCHANNELS - 1)
        (fn ch =>
         eprint ("Bitmap size for channel " ^ itos ch ^ ": " ^
                 (case Array.sub (bitmaps, ch) of
                    NONE => "NONE"
                  | SOME ga => itos (GA.length ga))))

      val () = Array.app gaussian_blur bitmaps

      val rate = Params.asint 44100 ratep
      val gain = Params.asreal 1.0 gainp
      val samples = mixaudio (gain, real rate)
      val () = eprint ("Rendered " ^ itos (Array.length samples) ^ " samples\n")

      (* Explicitly free bitmaps, which we don't need any more. *)
      val () = Util.for 0 (NCHANNELS - 1) (fn i => Array.update (bitmaps, i, NONE))

      fun sample16 (r : real) : int =
        let
          val r = if r > 1.0 then 1.0 else r
          val r = if r < ~1.0 then ~1.0 else r
        in
          Real.round (r * 32766.0)
        end
      val a = Vector.tabulate(Array.length samples,
                              fn i => sample16 (Array.sub (samples, i)))
    in
      eprint ("Writing " ^ OUTPUT ^ "...");
      Wave.tofile { frames = Wave.Bit16 (Vector.fromList [a]),
                    samplespersec = Word32.fromInt rate } OUTPUT
    end handle e as Hero s => (eprint ("FilterHero exception: " ^ s); raise e)
             | e as Wave.Wave s => (print ("WAVE ERROR: " ^ s ^ "\n"); raise e)
             | e =>
                let in
                    app (fn s => eprint (s ^ "\n")) (MLton.Exn.history e);
                    eprint ("Uncaught exception: " ^ exnName e ^ " / " ^ exnMessage e);
                    raise e
                end
end

val () = Params.main1 "Single MIDI file on the command line." FilterHero.render
