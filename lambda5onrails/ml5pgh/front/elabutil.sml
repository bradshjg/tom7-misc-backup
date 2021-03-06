
structure ElabUtil :> ELABUTIL =
struct

    exception Elaborate of string
    structure V = Variable

    infixr 9 `
    fun a ` b = a b

    val ltos = Pos.toString

    fun error loc msg =
        let in
            print ("Error at " ^ Pos.toString loc ^ ": ");
            print msg;
            print "\n";
            raise Elaborate "error"
        end

    val all_evars  = ref (nil : IL.typ IL.ebind ref list)
    val all_wevars = ref (nil : IL.world IL.ebind ref list)
    fun clear_evars () = (all_evars  := nil;
                          all_wevars := nil)
    fun finalize_evars () =
      let in
        app (fn r =>
             case !r of
               IL.Bound _ => ()
             | IL.Free _ => r := IL.Bound (IL.TRec nil)) (!all_evars);
        app (fn r =>
             case !r of
               IL.Bound _ => ()
             | IL.Free _ => r := IL.Bound Initial.home) (!all_wevars)
      end

    fun new_evar ()  = 
      let val e = Unify.new_ebind ()
      in
        all_evars := e :: !all_evars;
        IL.Evar e
      end
    fun new_wevar () =
      let val e = Unify.new_ebind ()
      in
        all_wevars := e :: !all_wevars;
        IL.WEvar e
      end

    (* XXX5 compile flag *)
    fun warn loc s =
      let in
        print ("Warning at " ^ Pos.toString loc ^ ": " ^ s ^ "\n")
      end

    fun wsubst1 w v t = Subst.wsubst (Subst.fromlist [(v,w)]) t

    (* unify context location message actual expected *)
    fun unify ctx loc msg t1 t2 =
            Unify.unify ctx t1 t2
            handle Unify.Unify s => 
                let 
                    val $ = Layout.str
                    val % = Layout.mayAlign
                in
                    Layout.print
                    (Layout.align
                     [%[%[$("Type mismatch (" ^ s ^ ") at "), %[$(Pos.toString loc),
                          $": "], $msg]],
                      %[$"expected:", Layout.indent 4 (ILPrint.ttolex (MuName.name ctx) t2)],
                      %[$"actual:  ", Layout.indent 4 (ILPrint.ttolex (MuName.name ctx) t1)]],
                     print);
                    print "\n";
                    raise Elaborate "type error"
                end

    (* unify context location message actual expected *)
    fun unifyw ctx loc msg w1 w2 =
            Unify.unifyw ctx w1 w2
            handle Unify.Unify s => 
                let 
                    val $ = Layout.str
                    val % = Layout.mayAlign
                in
                    Layout.print
                    (Layout.align
                     [%[$("World mismatch (" ^ s ^ ") at "), $(Pos.toString loc),
                        $": ", $msg],
                      %[$"expected:", Layout.indent 4 (ILPrint.wtol w2)],
                      %[$"actual:  ", Layout.indent 4 (ILPrint.wtol w1)]],
                     print);
                    print "\n";
                    raise Elaborate "type error"
                end
              
    local open Primop Podata
    in
      fun ptoil PT_INT = Initial.ilint
        | ptoil PT_STRING = Initial.ilstring
        | ptoil PT_BOOL = Initial.ilbool
        | ptoil (PT_VAR v) = IL.TVar v
        | ptoil (PT_REF p) = IL.TRef ` ptoil p
        | ptoil (PT_ARRAY p) = IL.TVec ` ptoil p
        | ptoil PT_UNIT = IL.TRec nil
        | ptoil PT_CHAR = Initial.ilchar
        | ptoil PT_UNITCONT = raise Elaborate "unimplemented potoil unitcont"
    end

    local open JSImports
    in
      fun jtoil G JS_EVENT = 
        (case Context.con G Initial.eventname of
           (0, IL.Typ t, IL.Regular) => t
         | _ => raise Elaborate "event is wrongly declared??")
        | jtoil G JS_CHAR = Initial.ilchar
    end

    val itos = Int.toString

    val newstr = ML5pghUtil.newstr

    (* This uses the outer context to figure out which evariables can be generalized. *)
    fun polygen ctx (ty : IL.typ) (atworld : IL.world) =
        let 
            val acct = ref nil
            val accw = ref nil

            val occurs_in_atworld =
              (* path compress first *)
              let 
                fun mkfun w =
                  case w of
                    IL.WEvar er =>
                      (case !er of
                         IL.Bound ww => mkfun ww
                       | IL.Free m => (fn n => n = m))
                  | _ => (fn _ => false)
              in
                mkfun atworld
              end

            fun gow w =
              (case w of
                 IL.WEvar er =>
                   (case !er of
                      IL.Bound ww => gow ww
                    | IL.Free n => 
                        if Context.has_wevar ctx n orelse occurs_in_atworld n
                        then w
                        else
                          let val wv = V.namedvar "wpoly"
                          in
                            accw := wv :: !accw;
                            er := IL.Bound (IL.WVar wv);
                            IL.WVar wv
                          end)
               | IL.WVar _ => w
               | IL.WConst _ => w)

            fun got t =
                (case t of
                     IL.TRef tt => IL.TRef ` got tt
                   | IL.TVec tt => IL.TVec ` got tt
                   | IL.Sum ltl => IL.Sum ` ListUtil.mapsecond (IL.arminfo_map got) ltl
                   | IL.Arrow (b, tl, tt) => IL.Arrow(b, map got tl, got tt)
                   | IL.Arrows al => IL.Arrows ` map (fn (b, tl, tt) => 
                                                      (b, map got tl, got tt)) al
                   | IL.TRec ltl => IL.TRec ` ListUtil.mapsecond got ltl
                   | IL.TVar v => t
                   | IL.TCont tt => IL.TCont ` got tt
                   | IL.TTag (tt, v) => IL.TTag (got tt, v)
                   | IL.Mu (n, vtl) => IL.Mu (n, ListUtil.mapsecond got vtl)
                   | IL.TAddr w => IL.TAddr (gow w)
                   | IL.Shamrock (w, tt) => IL.Shamrock (w, got tt)
                   | IL.At (t, w) => IL.At(got t, gow w)
                   | IL.Evar er =>
                         (case !er of
                              IL.Free n =>
                                  if Context.has_evar ctx n
                                  then t
                                  else
                                      let 
                                          val tv = V.namedvar (Nonce.nonce ())
                                      in
                                          acct := tv :: !acct;
                                          er := IL.Bound (IL.TVar tv);
                                          IL.TVar tv
                                      end
                            | IL.Bound ty => got ty))
        in
            { t = got ty, tl = rev (!acct), wl = rev (!accw) }
        end

    fun polywgen ctx (w as IL.WEvar er) =
      (case !er of
         IL.Free n =>
           if Context.has_wevar ctx n
           then
             let in
               (* print "no polywgen: occurs\n"; *)
               NONE
             end
           else
               let
                   val wv = V.namedvar (Nonce.nonce ()) (* "polyw" *)
               in
                   er := IL.Bound (IL.WVar wv);
                   (* print "yes polywgen\n"; *)
                   SOME wv
               end
       | IL.Bound w => polywgen ctx w)
      | polywgen ctx (w as IL.WConst s) = 
         let in
           (* print "no polywgen: const\n"; *)
           NONE
         end
      | polywgen _   (w as IL.WVar v) = 
         let in
           print ("no polywgen: var " ^ V.tostring v ^ "\n");
           NONE
         end

    fun evarizes (IL.Poly({worlds, tys}, mt)) =
        let
          (* make the type and world substitutions *)
            fun mkts nil m ts = (m, rev ts)
              | mkts (tv::rest) m ts =
              let val e = new_evar ()
              in
                mkts rest (V.Map.insert (m, tv, e)) (e :: ts)
              end

            fun mkws nil m ws = (m, rev ws)
              | mkws (tv::rest) m ws =
              let val e = new_wevar ()
              in
                mkws rest (V.Map.insert (m, tv, e)) (e :: ws)
              end

            val (wsubst, ws) = mkws worlds V.Map.empty nil
            val (tsubst, ts) = mkts tys V.Map.empty nil

            fun wsu t = Subst.wsubst wsubst t
            fun tsu t = Subst.tsubst tsubst t

        in
          (map (wsu o tsu) mt, ws, ts)
        end

    fun evarize (IL.Poly({worlds, tys}, mt)) = 
      case evarizes ` IL.Poly({worlds=worlds, tys=tys}, [mt]) of
        ([m], ws, ts) => (m, ws, ts)
      | _ => raise Elaborate "impossible"


  (* used to deconstruct bool, which the compiler needs internally.
     implemented for general purpose (except evars?) *)
  fun unroll loc (IL.Mu(m, tl)) =
      (let val (_, t) = List.nth (tl, m)
           val (s, _) =
               List.foldl (fn((v,t),(mm,idx)) =>
                           (V.Map.insert
                            (mm, v, (IL.Mu (idx, tl))), idx + 1))
                          (V.Map.empty, 0)
                          tl
       in
           Subst.tsubst s t
       end handle _ => error loc "internal error: malformed mu")
    | unroll loc _ =
      error loc "internal error: unroll non-mu"

  fun mono t = IL.Poly({worlds= nil, tys = nil}, t)

  local
    val mobiles = ref nil : (Context.context * Pos.pos * string * IL.typ) list ref

    (* Like Context.has_evar; if we have decided that an evar must be mobile,
       then we can't generalize it because we don't have bounded quantification
       over mobile types. *)
    fun mobiles_have_evar n =
        List.exists (fn (G, _, _, t) =>
                     raise Fail "XXX FIXME HERE"
                     ) (!mobiles)

    (* mobility test.
       if force is true, then unset evars are set to unit
       to force mobility.
       if force is false and evars are seen, then defer this type for later *)
    fun emobile G pos s force t =
      let

        (* argument: set of mobile type variables *)
        fun em G t =
          case t of
            IL.Evar (ref (IL.Bound t)) => em G t
          | IL.Evar (ev as ref (IL.Free _)) => 
              if force
              then 
                let in
                  warn pos (s ^ ": unset evar in mobile check; setting to unit");
                  ev := IL.Bound (IL.TRec nil);
                  true
                end
              else (mobiles := (G, pos, s, t) :: !mobiles; true)

          | IL.TVar v => Context.ismobile G v

          | IL.TRec ltl => ListUtil.allsecond (em G) ltl
          | IL.Arrow _ => false
          | IL.Arrows _ => false
          | IL.Sum ltl => List.all (fn (_, IL.NonCarrier) => true 
                                     | (_, IL.Carrier { carried = t, ...}) => em G t) ltl

          (* no matter which projection this is, all types have to be mobile *)
          | IL.Mu (i, vtl) => 
              let val G = foldr (fn (v, G) => Context.bindmobile G v) G ` map #1 vtl
              in ListUtil.allsecond (em G) vtl
              end

          (* assuming immutable. 
             there should be a separate array type *)
          | IL.TVec t => em G t
          (* continuations aren't mobile. They could do anything. *)
          | IL.TCont t => false
          | IL.TRef _ => false
          (* the representation of a tag is always portable, but the 
             tag type is only mobile if its body is mobile. This is
             because a tag is permission to possibly use an extensible
             type at that type. *)
          | IL.TTag (t, _) => em G t
          | IL.At _ => true
          | IL.TAddr _ => true
          | IL.Shamrock _ => true
      in
        em G t
      end


    fun notmobile ctx loc msg t =
      let 
        val $ = Layout.str
        val % = Layout.mayAlign
      in
        Layout.print
        (Layout.align
         [%[$("Error: Type is not mobile at "), $(Pos.toString loc),
            $": ", $msg],
          %[$"type:  ", Layout.indent 4 (ILPrint.ttolex (MuName.name ctx) t)]],
         print);
        print "\n";
        raise Elaborate "type error"
      end

  in
    fun clear_mobile () = mobiles := nil

    fun check_mobile () =
      let in
        List.app (fn (ctx, pos, msg, t) => 
                  if emobile ctx pos msg true t
                  then () 
                  else notmobile Context.empty pos msg t) (!mobiles);
        clear_mobile ()
      end
 
    fun require_mobile ctx loc msg t =
        if emobile ctx loc msg false t
        then ()
        else notmobile ctx loc msg t

  end

end