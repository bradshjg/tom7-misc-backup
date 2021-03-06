
structure PH =
struct

  (* Types that define Power Hour Machines, which
     completely describe a power hour algorithm. *)
  datatype cup = Up | Down | Filled

  (* What a player does on a turn when he sees a state other than None.
     (In None, the only legal action is to not drink and not pass.)
     After drinking at most once, he can pass the cup to any player in
     a state of his choice, but:
      - If the cup is filled and we didn't drink, it stays
        filled.
      - The result of the round cannot have more than one
        cup in a given location.
     Note that it is normal for the player to pass to himself. *)

  type action = { drink : bool,
                  place : int * cup }

  (* The starting state for the player, and the three actions that can
     be specified. Note that in the None case, the only legal action
     is to not drink and not pass, so this is not represented. *)
  datatype player = P of { start : cup option,
                           up : action,
                           down : action,
                           filled : action }

  type machine = player list

  datatype simulation =
    S of { cups : cup option array ref,
           (* Never changes during simulation *)
           players : player vector,
           (* How many drinks has the player consumed? *)
           drinks : int array,
           (* What round is it? 0-59 *)
           round : int ref }

  fun makesim (m : machine) =
    S { cups = ref (Array.fromList (map (fn P { start, ... } => start) m)),
        players = Vector.fromList m,
        drinks = Array.fromList (map (fn _ => 0) m),
        round = ref 0 }

  exception Illegal of string
  fun step (S { cups, players, drinks, round }) =
      let
          val newcups = Array.array (Array.length (!cups), NONE)
          fun oneplayer (i, P { up, down, filled, ... }) =
              case Array.sub (!cups, i) of
                  NONE => ()
                | SOME now =>
                  let
                      val { drink, place as (dest, next) } =
                          case now of
                              Up => up
                            | Down => down
                            | Filled => filled
                  in
                      (if drink
                       then Array.update (drinks, i,
                                          Array.sub (drinks, i) + 1)
                       else ());
                      (case Array.sub (newcups, dest) of
                          NONE => Array.update (newcups, dest, SOME next)
                        | SOME _ =>
                              raise (Illegal ("2+ cups at pos " ^
                                              Int.toString dest)))
                           handle Subscript => raise Illegal ("out of bounds")
                  end

      in
          Vector.appi oneplayer players;
          cups := newcups;
          round := !round + 1
      end

  datatype result =
      Finished of { drinks: int Array.array,
                    waste: int }
    | Error of { rounds : int, msg : string }

  fun exec (m : machine) =
    let
        val sim = makesim m
        fun oneround (s as S { round = ref 60, drinks, cups, ... }) =
            let
                (* Glasses filled at the end are waste. *)
                val waste = Array.foldl (fn (SOME Filled, b) => 1 + b
                                          | (_, b) => b) 0 (!cups)
            in
                Finished { drinks = drinks, waste = waste }
            end
          | oneround (s as S { round = ref round, ... }) =
            (step s; oneround s)
                 handle Illegal str => Error { rounds = round,
                                               msg = str }
    in
        oneround sim
    end

   val r = exec [P { start = SOME Up,
                     up = { drink = true, place = (0, Up) },
                     down = { drink = true, place = (0, Up) },
                     filled = { drink = true, place = (0, Up) } }]

   fun combine l k = List.concat (map k l)

   (* Returns a list of all the possible games with that
      many players *)
   fun allgames radix =
       let
           val cups = [Up, Down, Filled]
           val indices = List.tabulate (radix, fn i => i)
           val placements =
               combine indices (fn i =>
                                combine cups (fn c =>
                                              [(i, c)]))
           val plans =
               combine [true, false]
               (fn d =>
                combine placements
                (fn p => [{ drink = d, place = p }]))

           val players =
               combine (NONE :: map SOME cups)
               (fn start =>
                combine plans
                (fn u =>
                 combine plans
                 (fn d =>
                  combine plans
                  (fn f =>
                   [P { start = start, up = u, down = d, filled = f}]
                   ))))
           val () = print ("There are " ^
                           Int.toString (length players) ^
                           " different players.\n")
           (* Get all combinations of n players *)
           fun get 0 = [nil]
             | get n =
                 let val rest = get (n - 1)
                 in
                     combine players
                     (fn player =>
                      map (fn l => player :: l) rest)
                 end

           val res = get radix
           val () = print ("There are " ^
                           Int.toString (length res) ^
                           " different games.\n")
       in
           res
       end

   fun result_cmp (Finished _, Error _) = LESS
     | result_cmp (Error _, Finished _) = GREATER
     | result_cmp (Finished { drinks, waste },
                   Finished { drinks = dd, waste = ww }) =
       (case Int.compare (waste, ww) of
            EQUAL => Util.lex_array_order Int.compare (drinks, dd)
          | ord => ord)
     | result_cmp (Error { rounds, msg },
                   Error { rounds = rr, msg = mm }) =
       (case Int.compare (rounds, rr) of
            EQUAL => String.compare (msg, mm)
          | ord => ord)

   fun collate l =
       let
           val l = ListUtil.mapto exec l
           val l = map (fn (a, b) => (b, a)) l
           val l = ListUtil.stratify result_cmp l
       in
           l
       end

   fun ctos Up = "U"
     | ctos Down = "D"
     | ctos Filled = "F"

   fun atos { drink, place = (w,a) } =
       ctos a ^
       (if drink then "*" else "") ^
       "@" ^ Int.toString w

   fun playertostring (P { start, up, down, filled }) =
       "(start " ^
       (case start of
            NONE => "_"
          | SOME c => ctos c) ^ ", " ^
       StringUtil.delimit " " (map (fn (c, a) => ctos c ^ "=>" ^ atos a)
                               [(Up, up), (Down, down), (Filled, filled)]) ^
       ")"

   fun gametostring g =
       "[" ^ StringUtil.delimit "," (map playertostring g) ^ "]"

   fun restostring (Finished { drinks, waste }) =
       "[" ^
       StringUtil.delimit "," (map Int.toString
                               (Array.foldr op:: nil drinks)) ^
       "] wasting " ^
       Int.toString waste
     | restostring (Error { rounds, msg }) =
       "In " ^ Int.toString rounds ^ " round(s): " ^ msg

   fun show l =
       let
           fun showone (res, g) =
               print (restostring res ^ ": " ^
                      Int.toString (length g) ^ " game(s) like " ^
                      gametostring (hd g) ^ "\n")
       in
           app showone l
       end

   val () = show (collate (allgames 2))

end
