(* Representations:

   There is an array of all of the globals in the program.
   Labels are indices into this array.
   Global lams are themselves arrays, one for each fn in the bundle.
   
   A function value is a pair of integers: the first is the index of
   the global and the second is the index of the fn within the bundle.
   So function values are JS objects with prioperties g and f (integers).

   A value of type lams is just an integer, as is a value of type AllLam
   when there is at least one value arg.

   Static constructors are not represented at all, by definition. This includes
   At, Shamrock, WExists, ...

   A TPack agglomerates a dictionary and n values, so it is represented as
   a javascript object { d , v0, ..., v(-1) }

*)
structure JSCodegen :> JSCODEGEN =
struct

  infixr 9 `
  fun a ` b = a b

  exception JSCodegen of string

  open JSSyntax

  structure C = CPS
  structure SM = StringMap

  val codeglobal = Id.fromString "globalcode"
    
  (* maximum identifier length of 65536... if we have identifiers longer than that,
     then increasing this constant is the least of our problems. *)
  fun vartostring v = StringUtil.harden (fn #"_" => true | _ => false) #"$" 65536 ` Variable.tostring v

  val itos = Int.toString
  fun vtoi v = $(vartostring v)

  val pn = PropertyName.fromString

  fun wi f = f ` vtoi ` Variable.namedvar "x"

  fun generate { labels, labelmap } (gen_lab, SOME gen_global) =
    let

      (* convert the "value" value and send the result (exp) to the continuation k *)
      fun cvtv value (k : Exp.t -> Statement.t list) : Statement.t list =
        (case C.cval value of
           C.Var v => k ` Id ` vtoi v

         | C.UVar v => k ` Id ` vtoi v

         | C.Hold (_, va) => cvtv va k
         | C.Sham (_, va) => cvtv va k
         | C.WPack (_, va) => cvtv va k
         | C.Unroll va => cvtv va k
         | C.Roll (_, va) => cvtv va k
         (* if there are no vals, these are purely static *)
         | C.AllLam { worlds, tys, vals = nil, body } => cvtv body k
         | C.AllApp { worlds, tys, vals = nil, f } => cvtv f k

         | C.Dict _ => 
             wi (fn p => Bind (p, String ` String.fromString "dicts unimplemented") :: k ` Id p)

         | C.Int ic =>
             let val r = IntConst.toReal ic
             in k ` RealNumber r
             end
         | C.String s => wi (fn p => Bind (p, String ` String.fromString s) :: k ` Id p)
         | C.Codelab s =>
             (case SM.find (labelmap, s) of
                NONE => raise JSCodegen ("couldn't find codelab " ^ s)
              | SOME i => k ` Number ` Number.fromInt i)

         (* PERF we perhaps should have canonicalized these things a long time ago so
            that we always use the same label names. Though it's not obvious that arrays
            are even any more efficient than objects with property lists? *)
         | C.Record svl =>
             let
               val (ss, vv) = ListPair.unzip svl
             in
               cvtvs vv
               (fn vve =>
                let val svl = ListPair.zip (ss, vve)
                in
                  wi (fn p => 
                      Bind (p, Object ` % `
                            map (fn (s, v) =>
                                 Property { property = pn s,
                                            value = v }) svl) :: 
                      k ` Id p)
                end)
             end

         | C.Fsel (va, i) =>
             (* just a pair of the global's label and the function id within the bundle *)
             cvtv va
             (fn va =>
              wi (fn p => Bind (p, Object ` %[ Property { property = pn "g",
                                                          value = va },
                                              Property { property = pn "f",
                                                         value = Number ` Number.fromInt i } ])
                  :: k ` Id p))
              
         | C.Proj _ => 
             wi (fn p => Bind (p, String ` String.fromString "proj unimplemented") :: k ` Id p)

         | C.Inj _ => 
             wi (fn p => Bind (p, String ` String.fromString "inj unimplemented") :: k ` Id p)

         | C.AllApp _ => 
             wi (fn p => Bind (p, String ` String.fromString "allapp unimplemented") :: k ` Id p)

    (*
        | Fsel of 'cval * int
        | Proj of string * 'cval
        | Record of (string * 'cval) list
        | Inj of string * ctyp * 'cval option
        | AllLam of { worlds : var list, tys : var list, vals : (var * ctyp) list, body : 'cval }
        | AllApp of { f : 'cval, worlds : world list, tys : ctyp list, vals : 'cval list }

        | VLeta of var * 'cval * 'cval
        | VLetsham of var * 'cval * 'cval
    *)
         | C.Dictfor _ => raise JSCodegen "should not see dictfor in js codegen"
         | C.Lams _ => raise JSCodegen "should not see lams except at top level in js codegen"
         | C.AllLam { vals = _ :: _, ... } => 
             raise JSCodegen "should not see non-static alllam in js codegen"

         | C.TPack (_, _, vd, vl) =>
             cvtv vd 
             (fn (vde : Exp.t) =>
              cvtvs vl 
              (fn (vle : Exp.t list) =>
               wi (fn p =>
                   Bind (p, Object ` % `
                         (Property { property = PropertyName.fromString "d",
                                     (* XXX requires that it is an identifier, num, or string? *)
                                     value = vde } ::
                          ListUtil.mapi (fn (e, idx) =>
                                         Property { property = PropertyName.fromString ("v" ^ itos idx),
                                                    value = e }) vle))
                   :: k ` Id p
                   )))
         | C.VTUnpack (_, dv, cvl, va, vbod) =>
             cvtv va
             (fn ob =>
              (* select each component and bind... *)
              Bind(vtoi dv, SelectId { object = ob, property = Id.fromString "d" }) ::
              (ListUtil.mapi (fn ((cv, _), i) =>
                             Bind(vtoi cv, SelectId { object = ob, property = Id.fromString ("v" ^ itos i) }))
                            cvl @
               cvtv vbod k)
              )

         |  _ => 
             wi (fn p =>
                 Bind (p, String (String.fromString "unimplemented val")) ::
                 k ` Id p)
             )

    (*       [Throw ` String ` String.fromString "unimplemented val"]) *)


      and cvtvs (values : C.cval list) (k : Exp.t list -> Statement.t list) : Statement.t list =
           case values of 
             nil => k nil
           | v :: rest => cvtv v (fn (ve : Exp.t) =>
                                  cvtvs rest (fn (vl : Exp.t list) => k (ve :: vl)))

      fun cvte exp : Statement.t list =
           (case C.cexp exp of
              C.Halt => [Return NONE]

            | C.ExternVal (var, lab, _, _, e) =>
            (* XXX still not really sure how we interface with things here... *)
                (Var ` %[(vtoi var, SOME (Id ` Id.fromString lab))]) :: cvte e

            | C.Letsham (v, va, e)    => cvtv va (fn ob => Bind (vtoi v, ob) :: cvte e)
            | C.Leta    (v, va, e)    => cvtv va (fn ob => Bind (vtoi v, ob) :: cvte e)
            | C.WUnpack (_, v, va, e) => cvtv va (fn ob => Bind (vtoi v, ob) :: cvte e)

            | C.TUnpack (_, dv, cvl, va, e) =>
                cvtv va
                (fn ob =>
                 (* select each component and bind... *)
                 Bind(vtoi dv, SelectId { object = ob, property = Id.fromString "d" }) ::
                 (ListUtil.mapi (fn ((cv, _), i) =>
                                 Bind(vtoi cv, SelectId { object = ob, 
                                                          property = Id.fromString ("v" ^ itos i) }))
                  cvl @
                  cvte e))

            | C.Go _ => raise JSCodegen "shouldn't see go in js codegen"
            | C.Go_cc _ => raise JSCodegen "shouldn't see go_cc in js codegen"
            | C.ExternWorld _ => raise JSCodegen "shouldn't see externworld in js codegen"

            | C.Primop ([v], C.PRIMCALL { sym, ... }, args, e) =>
                cvtvs args
                (fn args =>
                 (* assuming sym has the name of some javascript function. *)
                 Bind (vtoi v, Call { func = Id ` $sym, args = %args }) ::
                 cvte e)

            | C.Primop _ => [Throw ` String ` String.fromString "unimplemented primop"]

            | C.Go_mar _ => [Throw ` String ` String.fromString "unimplemented go_mar"]

            | C.Put _ => [Throw ` String ` String.fromString "unimplemented put"]
            | C.Case _ => [Throw ` String ` String.fromString "unimplemented case"]

            | C.ExternType (_, _, vso, e) => [Throw ` String ` String.fromString "unimplemented externtype"]

(*
      (* post marshaling conversion *)
    | Go_mar of { w : world, addr : 'cval, bytes : 'cval }
    | Primop of var list * primop * 'cval list * 'cexp
    | Put of var * ctyp * 'cval * 'cexp
    (* typ var, dict var, contents vars *)
    | Case of 'cval * var * (string * 'cexp) list * 'cexp
*)


            | C.Call (f, args) => 
            (* XXX should instead put this on our thread queue. 
               We need to do something since tail calls are not constant-space! *)
            (* FIXME not f. it will be a pair of ints. get them from the code global *)
                cvtv f 
                (fn fe =>
                 cvtvs args
                 (fn ae =>
                  [Exp ` Call { func = fe, args = % ae }]))
                )



      (* Now, to generate code for this global... *)

      (* In this type-erasing translation there are a lot of things
         we'll ignore, including the difference between PolyCode and Code. *)
      val va = 
        (case C.cglo gen_global of
           C.PolyCode (_, va, _) => va
         | C.Code (va, _, _) => va)
    in
      (case C.cval va of
         C.AllLam { worlds = _, tys = _, vals, body } =>
           (case C.cval body of
              C.Lams fl =>
                Array ` % ` map SOME `
                map (fn (f, args, e) =>
                     let
                     in
                       Function { args = % ` map (vtoi o #1) args,
                                  name = NONE,
                                  body = % ` cvte e }
                     end) fl

            | C.AllLam _ => raise JSCodegen "hoisted alllam unimplemented"
            | _ => raise JSCodegen ("expected label " ^ gen_lab ^ 
                                    " to be alllam then lams/alllam"))

         | _ => raise JSCodegen ("expected label " ^ gen_lab ^ 
                                 " to be a (possibly empty) AllLam"))
    end
    | generate _ (lab, NONE) = 
    Function { args = %[],
               name = NONE,
               body = %[Throw ` String ` 
                        String.fromString "wrong world"] }

end