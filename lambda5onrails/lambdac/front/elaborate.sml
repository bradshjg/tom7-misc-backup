
structure Elaborate :> ELABORATE =
struct

  val sequenceunit = Params.flag false
    (SOME ("-sequence-unit", 
           "Require sequenced expressions and 'do' declarations to be of type unit")) "sequenceunit"

  val warnmatch = Params.flag true
    (SOME ("-warn-match",
           "(Conservative) warnings for non-exhaustive matches")) "warnmatch"

  infixr 9 `
  fun a ` b = a b
  fun I x = x

  structure V = Variable
  structure C = Context
  structure E = EL

  open IL
  open ElabUtil
  structure P = Primop

    (* XXX *)
    structure Pattern =
    struct
      fun elaborate _ _ _ _ _ _ = (raise Elaborate "pattern stub unimplemented") : IL.exp * IL.typ
      exception Pattern of string
    end

      
  exception Impossible

  (* XXX elabutil *)
  fun mkfn (args, dom, cod, body) =
      let val f = V.namedvar "fn"
      in
          (* only ok since mono *)
          Let(Fix(mono[{name=f,
                        arg=args,
                        dom=dom,
                        cod=cod,
                        inline=false,
                        total=false,
                        (* since the name is new, it is not recursive *)
                        recu=false,
                        body=body}]),
              Value (Var f))
      end

  fun mklist ctx loc t =
      (case C.con ctx Initial.listname of
           (1, Lambda f, _) => f [t]
         | _ => error loc "impossible: bad list type")


  (* ditto *)
  fun tuple l =
      let
          fun mktup _ nil = nil
            | mktup n (h::t) = 
              (Int.toString n, h) :: 
              mktup (n + 1) t
      in
          IL.TRec(mktup 1 l)
      end

  fun lookupt ctx loc str =
      (case C.con ctx str of
           (0, Typ t, _) => t
         (* nullary rewrites these, so it should be impossible *)
         | (0, Lambda f, _) => (* f nil *) error loc ("invariant violation: lookupt got Lambda for " ^ str)
         | (kind, _, _) => 
               error loc (str ^ " expects " ^ 
                          itos kind ^ " type argument"
                          ^ (if kind = 1 then "" else "s") ^ "."))
           handle C.Absent _ => error loc ("Unbound type name " ^ str)

  and elabw ctx loc w =
    (WVar ` C.world ctx w)
    handle C.Absent _ => error loc ("Unbound world name " ^ w)

  and elabt ctx loc t = elabtex ctx NONE loc t

  (* XXX no need for prefix any more *)
  and elabtex ctx prefix loc t =
      (case t of
         E.TVar str => lookupt ctx loc str
       | E.TApp (l, str) =>
             (* XXX there are no modvars any more *)
             ((case C.con ctx str of
                   (kind, Lambda f, _) =>
                       if kind = length l 
                       then f (map (elabtex ctx prefix loc) l) 
                       else error loc 
                           (str ^ " expects " ^ itos kind ^ " type argument"
                            ^ (if kind = 1 then "" else "s") ^ " -- gave " ^
                            itos (length l))
                 | _ => error loc (str ^ " is not a type constructor."))
                   handle Context.Absent _ => 
                       error loc ("Unbound type constructor " ^ str))
       | E.TRec ltl => let 
                           val ltl = ListUtil.sort 
                                     (ListUtil.byfirst HumlockUtil.labelcompare) ltl
                       in 
                           if ListUtil.alladjacent
                                (ListUtil.byfirst op<>) ltl 
                           then TRec (ListUtil.mapsecond (elabtex ctx prefix loc) ltl)
                           else error loc "Duplicate label in record type"
                       end
       | E.TNum n => if n >= 0
                     then 
                       TRec (List.tabulate(n, (fn i => (Int.toString (i + 1),
                                                        new_evar ()))))
                     else error loc "num type (record length) must be non-negative"
                       
       | E.TArrow (dom, cod) => Arrow (false, 
                                       [elabtex ctx prefix loc dom], 
                                       elabtex ctx prefix loc cod))

    and dovar ctx loc vv =
    ((case C.var ctx vv of
(* XXX5 not sure what to do with primitives. are there any?
      (pt, _, Primitive p) =>
        let 
            fun unpoly (Quant (v, pt)) ts = unpoly pt (v::ts)
              | unpoly (Mono (Arrow(_, dom, cod))) ts =
                let 
                    (* turn array into
                       (where 'a means an evar)

                       let fun array_105 (n : int, init : 'a) 
                           : 'a vec = 
                             Primapp(array, [n, init], ['a])
                       in
                           array_105
                       end

                       ... it will be inlined in the il
                       optimizer if applied directly to 
                       arguments. *)

                    (* XXX use elabutil.evarize *)
                    val x = V.namedvar "pa"

                    fun mkes nil = (nil, V.Map.empty)
                      | mkes (tv::rest) =
                        let val (ets, subst) = mkes rest
                            val ev = new_evar ()
                        in
                            (ev :: ets, 
                             V.Map.insert (subst, tv, ev))
                        end

                    val (ets, subst) = mkes ts

                    val ndom = map (Subst.tsubst subst) dom
                    val ncod = Subst.tsubst subst cod

                in 
                    case ndom of
                        [_] =>
                          (* single argument. function will
                             just take that. *)
                          (mkfn ([x],
                                 ndom,
                                 ncod,
                                 Primapp(p,
                                         [Var x],
                                         ets)),
                           Arrow(false, ndom, ncod))

                      | _ => 

                          (* many arguments -- function will
                             take a tuple of them *)
                          (mkfn 
                           ([x], [tuple ndom], ncod,
                            let
                                fun getargn n =
                                    Proj(Int.toString (n + 1),
                                         tuple ndom,
                                         Var x)
                            in
                                Primapp(p,
                                        List.tabulate
                                        (length ndom, getargn),
                                        ets)
                            end),
                           Arrow(false, [tuple ndom], ncod))
                end
              (* preclude prims of type Aa.a *)
              | unpoly (Mono (TVar any)) nil =
                    (Primapp (p, nil, nil), TVar any)
              | unpoly _ _ = error loc 
                    "BUG: Bad type of primop."
        in
            unpoly pt nil
        end

    | *)
      (pt, v, i, Context.Valid) =>
        let
          (* See below *)
          val (tt, worlds, tys) = evarize pt
        in
          (Polyuvar {tys = tys, worlds = worlds, var = v}, tt, new_wevar())
        end
    | (pt, v, i, Context.Modal w) =>
        let
            (* If polymorphic, instantiate the variable's 
               forall-quantified type variables with new 
               evars. Use type application on the expression 
               to record this. *)

            val (tt, worlds, tys) = evarize pt
        in
            (Polyvar {tys = tys, worlds = worlds, var = v}, tt, w)
        end) handle C.Absent _ => 
          error loc ("Unbound identifier: " ^ vv))

  and value (v, t) = (Value v, t)

  and elab ctx here ((e, loc) : EL.exp) =
      case e of
          E.Seq (e1, e2) => 
              let val (e1, e1t) = elab ctx here e1
                  val (e2, e2t) = elab ctx here e2
              in 
                (if !sequenceunit
                 then unify ctx loc "sequence unit" e1t (IL.TRec nil)
                 else ());
                (Seq(e1, e2), e2t)
              end

        | E.Var vv => 
              let 
                val (v, t, w) = dovar ctx loc vv
              in
                unifyw ctx loc "variable" here w;
                value (v, t)
              end

        | E.Get(addr, body) =>
              let
                val (aa, at) = elab ctx here addr
                val there = new_wevar ()
                val () = unify ctx loc "get addr" at (IL.TAddr there)
                val (bb, bt) = elab ctx there body
              in
                require_mobile ctx loc "get" bt;
                (Get (aa, bt, bb), bt)
              end

        | E.Constant(E.CInt i) => value (Int i, Initial.ilint)
        | E.Constant(E.CChar c) => value (Int (Word32.fromInt (ord c)), Initial.ilchar)

        | E.Float _ => error loc "Unsupported: floating point"

        | E.Constant (E.CString s) => value (String s, Initial.ilstring)

        | E.Vector nil => 
              let
                val t = new_evar()
              in
                (Primapp(P.PArray0, nil, [t]), TVec t)
              end

        (* XXX this is probably not good enough since it should be a value *)
        (* derived form *)
        | E.Vector (first::rest) =>
               let
                   (* [| x, y, z |]

                      becomes

                      let val a = array (3, x)
                      in
                          update(array, 1, y);
                          update(array, 2, z);
                          a
                      end *)

                   fun $x = (x, loc)
                   val n = 0w1 + Word32.fromInt (length rest)
                   val arr = newstr "arr"

                   fun dowrites _ nil e = e
                     | dowrites m (h::t) e =
                       $ ` 
                       E.Seq
                       ($ `
                        E.App($ ` E.Var "update_",
                              $ ` 
                              E.Record
                              [("1", $ ` E.Var arr),
                               ("2", $ ` E.Constant ` E.CInt m),
                               ("3", h)]),
                        dowrites (m + 0w1) t e)
               in
                   elab ctx here `
                   $ `
                    E.Let
                    ($ `
                     E.Val (nil, E.PVar arr,
                            $ `
                            E.App ($ ` E.Var "array",
                                   $ `
                                   E.Record
                                   [("1", $ ` E.Constant ` E.CInt n),
                                    ("2", first)])),
                     dowrites 0w1 rest ` $ ` E.Var arr)
               end

        | E.Throw _ => error loc "unimplemented: throw"
        | E.Letcc _ => error loc "unimplemented: letcc"
(* XXX5 support throw/letcc?
        | E.Throw (e1, e2) => 
               let
                 val (ee1, t1) = elab ctx e1
                 val (ee2, t2) = elab ctx e2
               in
                 (* thrown expression must equal cont type *)
                 unify ctx loc "throw" t2 (IL.TCont t1);
                 (Throw(ee1, ee2), new_evar ())
               end

        | E.Letcc (s, e) =>
               let
                    val cv = V.namedvar s
                    val bodt = new_evar ()
                    val ctx' = C.bindv ctx s (mono (IL.TCont bodt)) cv

                    val (ee, tt) = elab ctx' e
                      
               in
                 unify ctx loc "letcc" tt bodt;
                 (Letcc (cv, bodt, ee), bodt)
               end
*)

        | E.Jointext el =>
               let
                   val (ees, tts) = ListPair.unzip (map (elab ctx here) el)
               in
                   app (fn t => unify ctx loc "jointext" t Initial.ilstring) tts;

                   (Primapp(Primop.PJointext, ees, nil), Initial.ilstring)
               end

        | E.Record lel =>
               let
                   val letl = ListUtil.mapsecond (elab ctx here) lel
                   val _ = ListUtil.alladjacent (ListUtil.byfirst op<>) 
                              (ListUtil.sort 
                               (ListUtil.byfirst HumlockUtil.labelcompare) lel)
                           orelse error loc 
                              "Duplicate labels in record expression"
               in
                 (Record (map (fn (l, (e, t)) => (l, e)) letl),
                  TRec (map (fn (l, (e, t)) => (l, t)) letl))
               end

        | E.Proj (s, t, e) =>
               let
                   val (ee, tt) = elab ctx here e
                   val ttt = elabt ctx loc t
               in
                   unify ctx loc "proj" tt ttt;

                   case ttt of
                       TRec ltl =>
                           (case ListUtil.Alist.find op= ltl s of
                               NONE => error loc 
                                         ("label " ^ s ^ 
                                          " not in projection object type!")
                             | SOME tres => (Proj(s, ttt, ee), tres))
                     | _ => error loc "projection must be of record type"

               end

        | E.App(ef, ea) =>
               let 
                 val (ff, ft) = elab ctx here ef
                 val (aa, at) = elab ctx here ea
                 val dom = new_evar ()
                 val cod = new_evar ()
               in
                 unify ctx loc "app:function" ft (Arrow (false, [dom], cod));
                 unify ctx loc "app:arg" at dom;
                 (App (ff, [aa]), cod)
               end

        | E.Constrain(e, t, wo) =>
               let 
                   val (ee, tt) = elab ctx here e
                   val tc = elabt ctx loc t

                   val cw = (case wo of 
                               NONE => new_wevar ()
                             | SOME w => elabw ctx loc w)
               in
                   unify ctx loc "constraint" tt tc;
                   unifyw ctx loc "constrain:world" cw cw;
                   (ee, tc)
               end

        | E.Andalso (a,b) =>
               elab ctx here (E.If (a, b, Initial.falseexp loc), loc)

        | E.Orelse (a,b) =>
               elab ctx here (E.If (a, Initial.trueexp loc, b), loc)

        | E.Andthen (a, b) => 
               elab ctx here (E.If (a, (E.Seq (b, (E.Record nil, loc)), loc),
                                    (E.Record nil, loc)), loc)

        | E.Otherwise (a, b) => 
               elab ctx here (E.If (a, 
                                    (E.Record nil, loc),
                                    (E.Seq (b, (E.Record nil, loc)), loc)), loc)

        | E.If (cond, tt, ff) =>
               elab ctx here
               (E.Case ([cond],
                        [([Initial.truepat], tt),
                         ([Initial.falsepat], ff)], NONE), loc)

        | E.Case (es, m, default) =>
               let 
                 val def = case default of
                   SOME f => f
                 | NONE => 
                     fn () =>
                     let 
                       val warnstring = 
                         ("maybe inexhaustive match(case) at " ^ ltos loc)

                       val rexp = (EL.Raise (Initial.matchexp loc), loc)
                     in
                       if !warnmatch
                       then (EL.Seq((EL.CompileWarn warnstring, loc),
                                    rexp), loc)
                       else rexp
                     end

                   (* force case args to be variables, if they aren't. *)
                   fun force nil nc acc =
                            Pattern.elaborate true elab elabt nc loc
                                 (rev acc, m, def)
                     | force ((E.Var v, _)::rest) nc acc = 
                            force rest nc (v::acc)
                     | force (e::rest) nc acc =
                            let
                              val (ee, tt) = elab ctx here e
                              val s = newstr "case"
                              val sv = V.namedvar s
                              val nctx = C.bindv ctx s (mono tt) sv here
                              val (ein, tin) = force rest nctx (s::acc)
                            in
                              (Let(Val(mono(sv, tt, ee)),
                                   ein), tin)
                            end
               in
                   force es ctx nil
               end

        | E.Raise e =>
            (case C.con ctx Initial.exnname of
                 (0, Typ exnt, Extensible) =>
                     let 
                         val (ee, tt) = elab ctx here e
                         val ret = new_evar ()
                     in
                         unify ctx loc "raise" tt exnt;
                         (Raise (ret, ee), ret)
                     end
               | _ => error loc "exn type not declared???")

        | E.Handle (e1, pel) =>
            (case C.con ctx Initial.exnname of
              (0, Typ exnt, Extensible) =>
                let
                    val es = newstr "exn"
                    val ev = V.namedvar es
                        
                    val (ee, tt) = elab ctx here e1

                    val mctx = C.bindv ctx es (mono exnt) ev here

                    (* re-raise exception if nothing matches *)
                    fun def () =
                        (EL.Raise (EL.Var es, loc), loc)
                        
                    (* XXX5 and world.. *)
                    val (match, mt) = 
                        Pattern.elaborate true elab elabt mctx loc
                           ([es], ListUtil.mapfirst ListUtil.list pel, def)
                in
                    unify ctx loc "handle" tt mt;
                    (Handle(ee, ev, match), tt)
                end
            | _ => error loc "exn type not declared???")

        | E.CompileWarn s =>
               (Primapp(Primop.PCompileWarn s, [], []), TRec nil)

        (* makes slightly nicer code
           (nb, means that sequence-unit applies to do as well) *)
        | E.Let ((E.Do e, loc), e2) => elab ctx here (E.Seq(e, e2), loc)

        | E.Let (d, e) =>
               let
                   val (dd, nctx) = elabd ctx here d
                   val (ee, t) = elab nctx here e
               in
                   (foldr Let ee dd, t)
               end

  and mktyvars ctx tyvars =
      foldl (fn (tv, ctx) => C.bindc ctx tv (Typ ` new_evar ()) 0 Regular)
            ctx tyvars

  and elabf ctx 
            (here : world)
            (arg : string)
            (clauses : (EL.pat list * EL.typ option * EL.exp) list) 
            loc =
      let in
          (* ensure clauses all have the same length *)
          ListUtil.allpairssym (fn ((a, _, _), (b, _, _)) =>
                                length a = length b) clauses
               orelse error loc 
                  "clauses don't all have the same number of curried args";

          (* now we make big nested function *)

          (* p11 p12 p13 ... : t1 = e1 
             p21 p22 p23 ... : t2 = e2
              ...

             becomes:

             let fun f2 (x2) =
                 let fun f3 (x3) = ...

                     (case x , x2 , x3 of
                        p11 p12 p13 => e1
                        p21 p22 p23 => e2
                          ...
                        _ _ _ => raise Match)
                     : t1 : t2 : ... : tn 


                 in f3
                 end
             in f2 
             end

             *)
          (case clauses of
            [([pat], to, e)] =>
              let
                  (* base case *)
                  val (exp, tt) = 
                      Pattern.elaborate true elab elabt ctx loc
                         ([arg],
                          [([pat], e)],
                          (fn () =>
                           let 
                             val warnstring = 
                               ("maybe inexhaustive match(fun) at " ^ ltos loc)
                             val rexp = (EL.Raise (Initial.matchexp loc), loc)
                           in
                             if !warnmatch
                             then (EL.Seq((EL.CompileWarn warnstring, loc),
                                          rexp), loc)
                             else rexp
                           end))
              in
                  (case to of
                       SOME t => unify ctx loc 
                                   "codomain type constraint on fun" tt
                                   (elabt ctx loc t)
                     | _ => ());
                  (exp, tt)
              end
          | nil => raise Elaborate "impossible: *no* clauses in fn"
          | _ =>
               let
                   (* we already have an arg since we're inside the
                      function body, so that's where all this 'tl'
                      stuff comes from *)
                   val args = arg :: 
                       map (fn p =>
                            (case p of
                                 E.PVar s => newstr s
                               | E.PAs (s,_) => newstr s
                               | _ => newstr "cur")) ` tl ` #1 ` hd clauses

                   (* all constraints on fun body *)
                   val constraints =
                       List.mapPartial #2 clauses

                   val columns = map (fn (pl, _, e) => (pl, e)) clauses

                   fun buildf nil =
                       (* build the case, slapping all of the
                          body constraints on its outside *)
                       foldr (fn (t, e) => (E.Constrain(e, t, NONE (* XXX5 *)), loc)) 
                             (E.Case (map (fn a => (E.Var a, loc)) args, 
                                      columns, NONE), loc)
                             constraints
                     | buildf (x::rest) =
                       let
                           val fc = newstr "fc"
                       in
                           (E.Let((E.Fun [(nil, fc, [([E.PVar x], NONE,
                                                      buildf rest)])], loc),
                                  (E.Var fc, loc)),
                            loc)
                       end
               in
                   elab ctx here ` buildf ` tl args
               end)

      end handle Pattern.Pattern s => 
            error loc ("Pattern compilation failed: " ^ s)


  (* return a new context, and an il.dec list *)
  and elabd ctx (here : world) ((d, loc) : EL.dec) =  
    case d of
      E.Do e => 
        let val (ee, tt) = elab ctx here e
        in
          (if !sequenceunit
           then unify ctx loc "sequence unit" tt (IL.TRec nil)
           else ());
          ([Do ee], ctx)
        end

    | E.Type (nil, tv, typ) =>
          let val t = elabt ctx loc typ
          in ([], C.bindc ctx tv (Typ t) 0 Regular)
          end

    | E.Tagtype t =>
          let
              val tv = V.namedvar t
          in
              ([Tagtype tv], C.bindc ctx t (Typ (TVar tv)) 0 Extensible)
          end

    (* not like SML sigs. declares the availability of
       labels (later, type) without saying what they're bound to. *)
    (* XXX5 this code should perhaps be revived for 'extern' decls *)
(*
    | E.Signature (module, decs) =>
          let
              fun elabsdec ctx dd =
                  let fun checkdups atvs =
                      ListUtil.alladjacent op <> `
                      ListUtil.sort String.compare atvs
                      orelse 
                      error loc "duplicate type vars in signature dec"

                      fun sdec(atvs, var, ty) =
                          let
                              (* no dup tyvars *)
                              val _ = checkdups atvs
                                  
                              (* augment atvs with real variables too *)
                              val atvs = map (fn x => (x, V.namedvar x)) atvs
                                  
                              (* put tyvars in context for arms *)
                              val actx =
                                  foldl (fn ((s, x),c) =>
                                         C.bindc c s (Typ (TVar x)) 0 
                                         Regular) ctx atvs
                                  
                              (* now elaborate the type. *)
                              val tt = elabtex actx module loc ty
                                  
                              val pt = foldl Quant (Mono tt) (map #2 atvs)
                          in
                              pt
                          end

                  in
                    (case dd of
                         (* complications arise because of tyvars *)
                         E.SVal (atvs, var, ty) =>
                             let 
                                 val pt = sdec(atvs, var, ty)
                             in
                                 C.bindex ctx module var pt 
                                      (V.special module var) IL.Normal
                             end
                       | E.SPrim (atvs, var, ty, po) => 
                             let
                                 val pt = sdec(atvs, var, ty)
                                 val v = V.namedvar ("dummy_primop_" ^ var)
                             in
                                 C.bindex ctx module var pt v ` IL.Primitive po
                             end
                       | E.SType (atvs, t) => 
                             let
                               val _ = checkdups atvs
                               val v = V.special module t

                               val kind = length atvs
                               val con =
                                   Lambda 
                                   (fn l =>
                                    if length l <> kind
                                    then error loc 
                                         "(bug) wrong number of args to sdec Lambda"
                                    else TVar v)

                             in
                                 C.bindcex ctx module t con kind Regular
                             end)
                  end
          in
              (nil, foldl (fn (de, c) => elabsdec c de) ctx decs)
          end
*)

    | E.ExternWorld w => (nil, C.bindw ctx w (V.namedvar w))

    | E.ExternVal (atvs, id, ty, wo) =>
          let

            fun checkdups atvs =
              ListUtil.alladjacent op <> `
              ListUtil.sort String.compare atvs
              orelse 
              error loc ("duplicate type vars in extern val declaration (" ^ id ^ ")")
              
            (* no dup tyvars *)
            val _ = checkdups atvs
              
            (* augment atvs with real variables too *)
            val atvs = map (fn x => (x, V.namedvar x)) atvs
              
            (* put tyvars in context for elaboration of type *)
            val actx =
              foldl (fn ((s, x),c) =>
                     C.bindc c s (Typ (TVar x)) 0 
                     Regular) ctx atvs
              
            (* now elaborate the type. *)
            val tt = Poly ({ worlds = nil (* XXX5 *),
                             tys = map #2 atvs }, 
                           elabtex actx NONE loc ty)
              
            (* and world, if any *)
            val ww = Option.map (elabw ctx loc) wo

            val v = V.namedvar id

          in
            (nil,
             (* XXX5 should these be bound with different idstatus (Extern? Prim?) *)
             C.bindex ctx id tt v Normal (case ww of
                                            NONE => C.Valid
                                          | SOME w => C.Modal w))
          end

    (* some day we might add something to 'ty,' like a string list
       ref so that we can track the exception's history, or at least
       a string with its name and raise point. *)
    | E.Exception (e, ty) => elabd ctx here (E.Newtag(e, ty, Initial.exnname), loc)

    (* Tags must be local, because the embodied type might not be 
       mobile. *)
    | E.Newtag (tag, dom, ext) =>
       (case C.con ctx ext of
          (0, Typ (cod as TVar ev), Extensible) =>
            let
                (* need to generate the tag as well as
                   a constructor function *)
                val tagv = V.namedvar (tag ^ "_tag")
                val d = elabt ctx loc 
                     (case dom of
                          NONE => 
                              error loc 
                              "bug: nullary phase did not write newtag decl"
                        | SOME x => x)

                val ctor = V.namedvar tag
                val carg = V.namedvar "tagarg"

                (* don't put the tag in the context, since
                   user code should never access it. *)
                (* XXX not total for exns if we later add locality
                   info (probably a better translation would add
                   the locality info at the site of a Raise?) *)
                val nctx = C.bindex ctx tag 
                            (mono ` Arrow(true, [d], cod))
                            ctor
                            (Tagger tagv)
                            (C.Modal here)

            in
                ([Newtag (tagv, d, ev),
                  Fix ` mono
                      [{ name = ctor, arg = [carg],
                         dom = [d], cod = cod, 
                         (* PERF can't currently inline exn
                            constructors, because they are
                            open. -- see code in ilopt. *)
                         (* inline it! *)
                         inline = false,
                         recu = false, total = true,
                         body = Tag(Value ` Var carg, Value ` Var tagv) }]],
                 nctx)
            end
        | _ => error loc (ext ^ " is not an extensible type"))

    | E.Datatype (atvs, unsorted) =>
          let
            (* datatype (a, b, c) t = A of t | B of a | C of t1
               and                u = D of u | E of b | F

               syntax enforces uniformity restriction.
               prepass ensures every arm is 'of' some type.

                              tyvars       type
               Datatype of  string list * (string * 
                              ctor      members
                            (string * typ option) list) list
               *)

              (* put in sorted order. *)
              val dl =
                  ListUtil.sort (ListUtil.byfirst String.compare) unsorted

              (* check: no duplicate datatype names *)
              val _ =
                  ListUtil.alladjacent
                     (fn ((a, _), (b, _)) => a <> b) dl
                  orelse error loc "duplicate type names in datatype decl"

              (* check: no duplicate tyvars *)
              val _ =
                  ListUtil.alladjacent op <> `
                    ListUtil.sort String.compare atvs
                  orelse error loc "duplicate type vars in datatype decl"

              (* check: no overlap between tyvars and datatypes *)
              val _ =
                  ListUtil.alladjacent op <> `
                    ListUtil.sort String.compare (atvs @ map #1 dl)
                  orelse error loc 
                    "tyvar and datatype share same name in datatype decl"

              (* check: no duplicated constructors *)
              val _ =
                  app (fn (dt, arms) =>
                       let
                           val sorted =
                               ListUtil.sort (ListUtil.byfirst 
                                              String.compare) arms
                       in
                           ListUtil.alladjacent
                             (fn ((a,_),(b,_)) => a <> b) sorted
                           orelse error loc
                                  ("duplicated constructor in datatype " ^
                                   dt); ()
                       end) dl

              (* augment with index and recursive var *)
              (* XXX note that we use the basename of the variable
                 created in order to determine the external name for
                 this type during pretty printing. This is nasty, but
                 to get the best results, use 'dt' as the basename
                 for the bound var. *)
              val dl =
                  ListUtil.mapi (fn ((dt, arms), n) =>
                                 (dt, arms, n,
                                  V.namedvar (dt 
                                              (* ^ "_" ^ itos n ^ "_" *)))) dl

              (* augment atvs with real variables too *)
              val atvs = map (fn x => (x, V.namedvar x)) atvs

                  
              (* put tyvars in context for arms *)
                  
              val actx = 
                  foldl (fn ((s, x),c) =>
                         C.bindc c s (Typ (TVar x)) 0 Regular) ctx atvs

              (* bind each datatype name to the corresponding
                 recursive variable for the purpose of elaborating
                 the arms. *)

              val actx = 
                  foldl (fn ((dt, _, _, v), c) =>
                         C.bindc c dt (Typ (TVar v)) 0 Regular) actx dl

              fun gen_arminfo NONE = NonCarrier
                | gen_arminfo (SOME t) = 
                let
                  val tt = elabt actx loc t
                in
                  (* PERF this is very conservative now
                     (got the post-bug jitters), mainly
                     designed to catch the list datatype.
                     With the current backend it could be
                     extended to all sorts of stuff, like
                     ints, other definitely allocated
                     datatypes, etc...
                     *)
                  Carrier { carried = tt,
                            definitely_allocated = 
                            (case tt of
                               TRec (_ :: _ :: _) => true
                             | TVec _ => true
                             | TRef _ => true
                             | Arrow _ => true
                             | _ => false) }
                end

              (* elaborate each arm *)
              val dl =
                  map (fn (dt, arms, n, v) =>
                       (dt, 
                        ListUtil.mapsecond gen_arminfo arms,
                        n, v)) dl

              (* make body of mu *)
              val mubod =
                  map (fn (_, arms, _, v) => (v, Sum arms)) dl
                  
              (* make il con for each, consuming n *)
              val dl = 
                  map (fn (dt, arms, n, v) =>
                       (dt, arms, Mu(n, mubod), v)) dl
                       

              (* outside the mu, we need to substitute the mu
                 itself for the datatype type variable. *)
              val musubst =
                  Subst.tsubst `
                  Subst.fromlist
                  (map (fn (_, _, mu, v) =>
                        (v, mu)) dl)

              (* don't need v any more *)
              val dl = map (fn (dt, arms, mu, _) => (dt, arms, mu)) dl

              (* generate the wrapper Lambda that binds the
                 tyvars *)

              val kind = length atvs

              (* XXX from here on I only use #2 of atvs... *)

              fun wrapper body =
                  let
                      fun bs nil nil s = s
                        | bs ((_,v)::vrest) (tt::trest) s =
                          bs vrest trest ` V.Map.insert (s, v, tt)
                        | bs _ _ _ =
                          error loc 
                            "wrong number of type arguments to datatype"
                  in
                      Lambda (fn tl => 
                              Subst.tsubst (bs atvs tl V.Map.empty) body)
                  end

              (* setup done. now generate the new context and decls. *)

              (* first add the datatypes *)
              val nctx = 
                  foldl
                   (fn ((dt, _, mu), c) =>
                    C.bindc c dt (wrapper mu) kind Regular) ctx dl

              (* now bind all the constructors *)

              val atvs = map #2 atvs
                  

              (* generate a list:
                 (ctor string, ctor var, ctor type, ctor decl) *)
              val ctors = 
                  List.concat `
                  map (fn (dt, arms, mu) =>
                       map (fn (ctor, NonCarrier) => 
                            let
                              val cty = Poly({worlds=nil, tys=atvs}, mu)

                              val v = V.namedvar ("ctor_null_" ^ ctor)
                            in
                              (ctor, v, cty,
                               Val `
                               Poly({worlds=nil, 
                                     tys=atvs},
                                    (v, mu, Inject(Sum arms, ctor, NONE))))
                            end
 
                             | (ctor, Carrier { carried = ty, ... }) =>
                            let 
                                val dom = musubst ty

                                val cty = 
                                  Poly({worlds=nil,
                                        tys=atvs},
                                       (Arrow 
                                        (true (* yes total! *), 
                                         [dom], mu)))

                                val ctorf = V.namedvar ("ctor_" ^ ctor)
                                val x = V.namedvar "xdt"
                            in
                                (ctor, ctorf,
                                 (* type of constructor *)
                                 cty,
                                 (* injection value *)
                                 Fix `
                                 Poly({worlds=nil, tys=atvs},
                                     [{ name = ctorf,
                                        dom = [dom],
                                        cod = mu,
                                        arg = [x],
                                        (* inline it! *)
                                        inline = true,
                                        recu = false,
                                        total = true,
                                        body =
                                        Roll(mu,
                                             Inject
                                             (Sum arms, ctor, SOME ` Value ` Var x))}]))

                            end) arms) dl

              (* bind the constructors. Constructors should be valid. *)
              val nctx =
                  foldl
                  (fn ((ctor, v, at, _),c) =>
                   C.bindex c ctor at v Constructor C.Valid) nctx ctors

          in
              (map #4 ctors, nctx)
          end

    | E.Type (tyvars, tv, typ) =>
          let
              val kind = length tyvars

              fun conf l =
                   if length l <> kind
                   then error loc "(bug) wrong number of args to Lambda"
                   else 
                       let val nc = 
                           ListPair.foldl 
                              (fn (tv, t, ctx) =>
                               C.bindc ctx tv (Typ t) 0 Regular) 
                              ctx (tyvars, l)
                       in
                           elabt nc loc typ
                       end
          in
              (* provisional instantiation of type, to make sure its
                 body is not bogus (unelaboratable) prima facie *)
              ignore ` conf (List.tabulate (kind, fn _ => new_evar ()));

              ListUtil.allpairssym op<> tyvars 
                 orelse error loc "duplicate tyvars in type dec";
              ([], C.bindc ctx tv (Lambda conf) kind Regular)
          end

    (* XXX5 it ought to be possible to write valid functions with this
       syntax or a syntax like it. *)
    | E.Fun bundle =>
          let

              val outer_context = ctx

              val  _ = List.null bundle 
                         andalso error loc "BUG: *no* fns in bundle?"

              val _ = ListUtil.allpairssym (fn ((_, f, _), (_, g, _)) =>
                                            f <> g) bundle
                         orelse error loc 
                             "duplicate functions in fun..and"

              (* make var for each fn *)
              val binds =
                  map (fn (tv, f, b) => 
                       let val vv = V.namedvar f
                           val dom = new_evar ()
                           val cod = new_evar ()
                       in
                           (tv, f, b, vv, dom, cod)
                       end) bundle

              (* to process the body of one function, we
                 bind the existential variables that appeared
                 before the function name, and bind all functions
                 at (mono) existential arrow type. *)
              fun onectx c tv =
                  let
                      val nc = 
                          foldl (fn ((_, f, _, vv, dom, cod), ct) =>
                                 C.bindv ct f (mono (Arrow(false, 
                                                            [dom],
                                                            cod))) vv here)
                                c binds
                  in
                      mktyvars nc tv
                  end

              (* for each function, elaborate and return:
                 name, var,
                 arg variable,
                 domain type
                 codomain type
                 il expression body *)
              fun onef (tv, f, clauses, vv, dom, cod) =
                  let
                      val c = onectx ctx tv
                      val x = newstr "x"
                      val xv = V.namedvar x
                      val nc = C.bindv c x (mono dom) xv here

                      val (exp, tt) = elabf nc here x clauses loc
                  in
                      unify c loc "fun body/codomain" tt cod;
                      (f, vv, xv, dom, cod, exp)
                  end

              (* collect up elaborated functions and
                 forall-quantified vars *)
              fun folder ((f, vv, x, dom, cod, exp),
                          (fs, efs, polys)) =
                  let
                      val (dom, dp) = polygen outer_context dom
                      val (cod, cp) = polygen outer_context cod
                  in
                      ({ name = vv,
                         arg = [x],
                         dom = [dom],
                         inline = false,
                         recu = true,
                         total = false,
                         cod = cod,
                         body = exp } :: fs, 
                       (f, vv, Arrow(false, [dom], cod)) :: efs,
                       dp @ cp @ polys)
                  end

              val (fs, efs, ps) = 
                  foldl folder (nil, nil, nil) ` map onef binds

              (* FIXME5. Ought allow polymorphism over worlds. *)
              fun mkpoly ps at = Poly({worlds=nil, (* XXX5 *)
                                       tys = ps}, at)

              (* rebuild the context with these functions
                 bound polymorphically *)
              fun mkcontext ((f, vv, at), cc) =
                  C.bindv cc f (mkpoly ps at) vv here

          in
              ([Fix ` mkpoly ps fs], foldl mkcontext ctx efs)
          end

    | E.Val (tyvars, pat, exp) =>
          let
              (* simply bind tyvars;
                 let generalization actually determine the type. 
                 if we want to have these type vars act like SML,
                 we can check after the decls are over that each
                 one is still free and generalizable. 
                 XXX we let these sit in the exported context --
                     something needs to be done about that!
                 *)
              val nctx = mktyvars ctx tyvars

              (* epat ctx p ee t
                 in the context ctx,

                 elaborate the pattern p : t = ee
                 *)
              fun 
                  (* we treat var patterns as (v as _),
                     so this optimization prevents us from
                     generating "do v" for each var binding *)
                  epat ctx EL.PWild (Value (Polyvar _)) tt = ([], ctx)
                | epat ctx EL.PWild ee tt = ([Do ee], ctx)
                | epat ctx (EL.PVar v) ee tt =
                  (* Did you know val x = e is just syntactic sugar 
                     for val x as _ = e ? *)
                  epat ctx (EL.PAs (v, EL.PWild)) ee tt
                | epat ctx (EL.PConstrain (p, t)) ee tt =
                  let in
                      unify ctx loc "val constraint" tt 
                         ` elabt ctx loc t;
                      epat ctx p ee tt
                  end
                | epat ctx (EL.PAs (v, p)) ee tt =
                  let

                      (* [val (x as p) = e]
                           is
                         val x = e     @
                         [val p = x]
                         *) 


                      (* XXX5 polygen! *)

                      val vv = V.namedvar v

                      (* put in context as mono var *)
                      val ctx = C.bindv ctx v (Poly({worlds=nil,
                                                     tys=nil}, tt)) vv here

                      (* FIXME: this looks broken. we should be
                         making a polyvar here (?) *)
                      val (ds, c) = epat ctx p (Value ` Var vv) tt
                  in
                      ([Val ` mono (vv, tt, ee)] @ ds,
                       c)
                  end
                (* if the exp is valuable (particularly, a var), 
                   we're all set. XXX5 could match just Values? *)
                | epat ctx (EL.PRecord spl) (ee as (Value (Polyvar _))) tt =
                  let
                      val tys = map (fn (s,_) => (s,new_evar ())) spl

                      (* recursively elaborate a 
                             val p = #label record_var
                         for each component *)
                      fun f ctx nil nil = (nil, ctx)
                        | f ctx ((s,p)::rest) ((_,t)::trest) =
                          let 
                              val (ds, c) = epat ctx p 
                                              (Proj (s, tt, ee)) t
                              val (rds, rc) = f c rest trest
                          in
                              (ds @ rds, rc)
                          end
                        | f _ _ _ = raise Impossible
                  in
                      (* make sure rhs has record type 
                         with the right fields *)
                      unify ctx loc "record pattern" tt (TRec tys);
                      f ctx spl tys
                  end
                | epat ctx (p as (EL.PRecord spl)) ee tt =
                  (* use as to bind tuple to a variable, 
                     then enter case above *)
                  epat ctx (EL.PAs (newstr "rec", p)) ee tt
                | epat _ _ _ _ = 
                    error loc "patterns in val dec must be irrefutable"

              val (ee, tt) = elab ctx here exp
          in
              epat nctx pat ee tt
          end

  fun elaborate el = 
    let
      val () = clear_mobile ()
      val (ee, tt) = elab Initial.initial Initial.home el
    in
      check_mobile ();
      (ee, tt)
    end

end
