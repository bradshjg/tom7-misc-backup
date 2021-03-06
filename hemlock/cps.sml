
structure CPS =
struct

    (* This datatype adapted from "The CPS datatype", pp. 12, 
       in Appel's "Compiling With Continuations". *)

    (* I am targetting a typed assembly language, and so I need
       to "preserve" typing information. I "do" this by having one
       type (call it "thing") and arranging that every value is
       a thing and all operations act on things and produce things.
       (This method is expedient and makes marshalling easy, but
       only a temporary solution!)

       We can represent this type when we re-enter a typed setting by
       creating a unified recursive sum type covering all the types
       that the primitives need. We also want n-tuples, but this would
       lead to an infinite sum type (for all possible lengths of thing
       * thing * thing ...). Instead, we cut off at some number, then
       chain anything longer. Note that we don't need to do this for
       n-ary sums. (All sums are represented by int * thing). *)

    (* Representation invariant: No variable is bound in more than
       one place. (This implies no shadowing as well.) *)

    (* when creating a record, supply a tag to indicate what it
       holds. Other things can be tagged, like function values,
       but you don't create those explicitly. *)
    datatype tag =
        INT
      | STRING
      | REF
      (* code address *)
      | CODE
      (* int * t. Used to tag object language sums. *)
      | INT_T of int
      (* tuple of the supplied length. Limited to ??? *)
      | TUPLE of int

    type var = Variable.var
        
    (* Immediate operands to the CPS instructions. *)
    datatype value = 
        Var of var
        (* function name after closure conversion *)
      | Label of var
        (* These are used initially to make constant
           folding easier, but are eventually transformed
           into allocations of records with the tags
           above. *)
      | Int of int
      | String of string

    type primop = Primop.cpsprimop

    datatype cexp =
        Alloc of tag * value list * var * cexp
      (* 0 based, int < ToCPS.MAXRECORD *)
      | Project of int * value * var * cexp
      | App of value * value list
      | Fix of (var * var list * cexp) list * cexp

      (* value should be an int.
         if it matches any of the ints in the list,
         proceed to that continuation, otherwise
         use the default continuation.
         *)
      | Intswitch of value * (int * cexp) list * cexp

      (* Same, except use on INT_T values. The var is bound
         to the T inside the value when the branch is taken
         (this now includes the default). *)
      | Sumswitch of value * var * (int * cexp) list * cexp

      (* where almost everything goes on... *)
      | Primop of primop * value list * var list * cexp list

      (* delayed memo cell. 
         XXX should probably just be unit -> cexp. *)
      | Deferred of unit -> cexp Util.Oneshot.oneshot

end