(* primop data *)
structure Podata =
struct

  exception Podata of string

  local open Primop
        structure I = IL
        fun mono t = I.Poly ({worlds = nil, tys = nil}, t)
  in

    (* nb: all operations are UNSIGNED, including comparisons
       (XXX5 actually we have not resolved this for ML5)
       *)
    fun tostring (B PTimes) = "Times"
      | tostring (B PPlus) = "Plus"
      | tostring (B PMinus) = "Minus"
      | tostring (B PDiv) = "Div"
      | tostring (B PMod) = "Mod"
      | tostring (B (PCmp PEq)) = "Eq"
      | tostring (B (PCmp PNeq)) = "Neq"
      | tostring (B (PCmp PLess)) = "Less"
      | tostring (B (PCmp PLesseq)) = "Lesseq"
      | tostring (B (PCmp PGreater)) = "Greater"
      | tostring (B (PCmp PGreatereq)) = "Greatereq"

      | tostring (PNull) = "Null"

      | tostring (B PAndb) = "Andb"
      | tostring (B PXorb) = "Xorb"
      | tostring (B POrb) =  "Orb"
      | tostring (B PShl) = "Shl"
      | tostring (B PShr) = "Shr"

      | tostring PNotb = "Notb"

      | tostring PEqs = "Eqs"

      | tostring PPrint = "Print"

      | tostring PNeg = "Neg"

      | tostring PSet = "Set"
      | tostring PGet = "Get"
      | tostring PRef = "Ref"

      | tostring PArray = "Array"
      | tostring PArray0 = "Array0"
      | tostring PSub = "Sub"
      | tostring PUpdate = "Update"
      | tostring PArraylength = "Arraylength"
      | tostring (PJointext i) = ("Jointext_" ^ Int.toString i)

      | tostring PBind = "Bind"
      | tostring PNewtag = "Newtag"
      | tostring PGethandler = "Gethandler"
      | tostring PSethandler = "Sethandler"

      | tostring (PCompileWarn s) = "Warn(" ^ s ^ ")"

      | tostring PPutc = "Putc"
      | tostring PGetc = "Getc"

      | tostring PHalt = "Halt"

      | tostring PShowval = "(DEBUG:ShowVal)"

    fun potype (PJointext i) = I.Poly({worlds = nil, tys = nil}, 
                                      (List.tabulate (i, fn _ => Initial.ilstring),
                                       Initial.ilstring))
      | potype PHalt = 
      let val a = Variable.namedvar "a"
      in I.Poly({worlds = nil, tys = [a]}, (nil, IL.TVar a))
      end
      | potype (B (PCmp _)) = mono ([Initial.ilint, Initial.ilint], Initial.ilbool)
      | potype (B PTimes) = mono ([Initial.ilint, Initial.ilint], Initial.ilint)
      | potype (B PPlus) = mono ([Initial.ilint, Initial.ilint], Initial.ilint)
      | potype (B PMinus) = mono ([Initial.ilint, Initial.ilint], Initial.ilint)
      | potype PEqs = mono ([Initial.ilstring, Initial.ilstring], Initial.ilbool)

      | potype p = raise Podata ("unimplemented potype " ^ tostring p)

    (* XXX this should probably go. *)
    fun polab (PJointext i) = "po_jointext_" ^ Int.toString i
      | polab PHalt = "po_halt" (* ? should be implemented internally, probably *)
      | polab (B (PCmp PEq)) = "po_eq" (* XXX also internal... *)
      | polab p = raise Podata ("unimplemented polab " ^ tostring p)

    val alist =
      [
      ("Times", B PTimes),
      ("Plus", B PPlus),
      ("Minus", B PMinus),
      ("Div", B PDiv),
      ("Mod", B PMod),
      ("Eq", B (PCmp PEq)),
      ("Neq", B (PCmp PNeq)),
      ("Less", B (PCmp PLess)),
      ("Lesseq", B (PCmp PLesseq)),
      ("Greater", B (PCmp PGreater)),
      ("Greatereq", B (PCmp PGreatereq)),
      ("Null", PNull),
      ("Andb", B PAndb),
      ("Xorb", B PXorb),
      ("Orb", B POrb),
      ("Shl", B PShl),
      ("Shr", B PShr),
      ("Notb", PNotb),
      ("Eqs", PEqs),
      ("Print", PPrint),
      ("Neg", PNeg),
      ("Set", PSet),
      ("Get", PGet),
      ("Ref", PRef),
      ("Array", PArray),
      ("Array0", PArray0),
      ("Sub", PSub),
      ("Update", PUpdate),
      ("Arraylength", PArraylength),
      ("Bind", PBind),
      ("Newtag", PNewtag),
      ("Gethandler", PGethandler),
      ("Sethandler", PSethandler),
      ("Putc", PPutc),
      ("Getc", PGetc),
      ("Halt", PHalt),
      ("Showval", PShowval)]

      (* nb. jointext and compilewarn are missing *)

    fun fromstring s =
      case ListUtil.Alist.find op= alist s of
        NONE => NONE (* XXX should allow at least jointext here *)
      | SOME po => SOME po

  end (* local *)
end