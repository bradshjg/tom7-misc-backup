
structure FSUtil :> FSUTIL =
struct

  exception FSUtil of string

  fun splitext s =
    StringUtil.rfield (StringUtil.ischar #".") s

  fun chdir_excursion s f =
    let val { arcs, isAbs, vol } = OS.Path.fromString s
    in
      case rev arcs of
        nil => raise FSUtil "no file in chdir_excursion"
      (* don't need to move *)
      | [file] => f file
      (* move to dir and come back *)
      | (file :: rest) =>
          let
            val new = OS.Path.toString
              { arcs = rev rest, isAbs = isAbs, vol = vol }
            val old = OS.FileSys.getDir ()
          in
            let in
              OS.FileSys.chDir new;
              f file before
              OS.FileSys.chDir old
            end handle ex => (OS.FileSys.chDir old; raise ex)
          end
    end

  (* assume exists = can open. This is probably not the best implementation.. *)
  fun exists f =
      (BinIO.closeIn (BinIO.openIn f); true) handle _ => false

  fun tempfilename base =
    let
      fun tf n =
        let val f = base ^ Int.toString n ^ ".tmp"
        in
          if exists f
          then tf (n + 1)
          else f
        end
    in
      tf 0
    end

  (* imperative streams *)
  type 'a stream = unit -> 'a option
  (* but we want to implement them in a more efficient way *)
  datatype 'a prestream =
    PS of unit -> ('a * 'a prestream) option

  fun ps_to_stream (PS ps) =
    let
      val r = ref ps
      fun next () =
        (case (!r) () of
           NONE =>
             let in
               r := (fn _ => NONE);
               NONE
             end
         | SOME (a, PS af) =>
             let in
               r := af;
               SOME a
             end)
    in
      next
    end

  structure FS = Posix.FileSys

  type fileinfo = { dir : bool,
                    lnk : bool,
                    mode : FS.S.mode,
                    nlink : int,
                    mtime : Time.time,
                    size : Position.int,
                    uid : FS.uid,
                    gid : FS.gid,
                    name : string }

  local open FS.S
  in
    fun modestring m =
      let
        fun t d c = if anySet (flags [d], m) then c else #"-"
      in
        implode [t irusr #"r",
                 t iwusr #"w",
                 t ixusr #"x",
                 t irgrp #"r",
                 t iwgrp #"w",
                 t ixgrp #"x",
                 t iroth #"r",
                 t iwoth #"w",
                 t ixoth #"x"]
      end
  end

  fun stream_app f s =
    (case s () of
           NONE => ()
         | SOME a =>
             let in
               ignore (f a);
               stream_app f s
             end)

  val ps_nil = PS (fn _ => NONE)

  fun ps_cons a f2 = PS (fn () => SOME(a, f2))

  fun ps_delay ups =
    PS (fn () =>
        case ups () of
          PS f => f ())

  fun forcestream s =
    case s () of
      NONE => nil
    | SOME item => item :: forcestream s

(*
  fun dirplus "" p = p
    | dirplus d p =
      case CharVector.sub(d, size d - 1) of
          #"/" => d ^ p
        | _    => d ^ "/" ^ p
*)
  (* works on Windows too. *)
  fun dirplus d p = OS.Path.concat (d, p)

  local open FS
  in

    fun dirstream s =
      let
        (* opendir doesn't raise an exception
           if the directory doesn't exist? *)
        val _ = stat s
        val d = opendir s
        fun re e nm =
          let
            val st = stat (dirplus s e)
          in
            { dir = ST.isDir st,
              lnk = ST.isLink st,
              mode = ST.mode st,
              nlink = ST.nlink st,
              mtime = ST.mtime st,
              size = ST.size st,
              uid = ST.uid st,
              gid = ST.gid st,
              name = nm }
          end

        (* PORT readdir has changed types in the new basis.
           Uncomment this for old basis versions. Comment
           out for new basis versions. *)

       (*
        val readdir =
            (fn dd => (case readdir d of
                           "" => NONE
                         | xx => SOME xx) handle _ => NONE)
            *)


        fun loop NONE () = (closedir d; ps_nil)
          | loop (SOME ss) () =
            let
              val tail = ps_delay (loop (readdir d))
            in
              ps_cons (re ss ss) tail
            end handle _ => loop (readdir d) ()


        (* XXX what if I can't stat ..? *)
        val all =
          ps_delay (fn () =>
                    ps_cons
                    (re "" ".")
                    (ps_delay (fn () =>
                               ps_cons
                               (re ".." "..")
                               (ps_delay (loop (readdir d))) )))
      in
        ps_to_stream all
      end

    (* val dirstream = ps_to_stream dirstream *)

    fun dirapp f s = stream_app f (dirstream s)

  end

  fun stream_filter f s () =
    case s () of
      NONE => NONE
    | SOME item =>
        case f item of
          false => stream_filter f s ()
        | true => SOME item

  fun stream_map f s () =
    case s () of
      NONE => NONE
    | SOME item => SOME (f item)

  (* Note: If directory globbing is ever implemented, this
     should memoize (or lazily limit its result set) so that
     the classic ../*/../*/../*/../*/../*/../*/.. DoS won't work. *)
  fun glob_in_dir dir filemask =
    let
      val stream = dirstream dir
    in
      stream_filter
      (fn {name, ...} =>
       StringUtil.wcmatch filemask name) stream
    end

  (* Returns the optional directory prefix, and fileinfo stream. *)
  fun glob_internal s =
    let
      (* Allow for both \ and / directory separators on windows. *)
      val s = OS.Path.toUnixPath s
    in
      case StringUtil.rfind "/" s of
        NONE =>
          (NONE, stream_filter (fn {name, ...} =>
                                StringUtil.wcmatch s name) (dirstream "."))
      | SOME i =>
          let
            val dir = String.substring (s, 0, i)
            val filemask = String.substring (s, i + 1, size s - (i + 1))
          in
            (SOME dir, glob_in_dir dir filemask)
          end
    end

  fun glob s = #2 (glob_internal s)

  fun globfiles s =
    let
      val (dir, stream) = glob_internal s
      (* Only take files. *)
      val stream = stream_filter (fn { dir, ... } => not dir) stream
      val items = forcestream stream
    in
      case dir of
        NONE => map (fn { name, ... } => name) items
      | SOME dir => map (fn { name, ... } => dirplus dir name) items
    end

  fun ls f s =
    if StringUtil.all (StringUtil.isn'tchar #"*") s then
       dirapp f s
    else
      stream_app f (glob s)

  exception Seek

  fun seekn x s =
    ignore (size (TextIO.inputN (s, x)) <> x andalso raise Seek)

  fun skipi 0 _ = ()
    | skipi n s =
      if n < 0 then raise Seek
      else if n < 32 then let in ignore (TextIO.input1 s); skipi (n - 1) s end
           else if n < 128 then let in seekn 32 s; skipi (n - 32) s end
                else if n < 1024 then let in seekn 128 s; skipi (n - 128) s end
                     else let in seekn 1024 s; skipi (n - 1024) s end

  fun filesize64 f = (FS.ST.size (FS.stat f))
  fun filesize f = Position.toInt (filesize64 f)

end