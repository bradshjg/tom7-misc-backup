(* Copyright 2010 Tom Murphy VII and Erin Catto. See COPYING for details. *)

(* Time of impact calculation.
   Corresponding to common/b2timeofimpact.cpp. *)
structure BDDTimeOfImpact :> BDDTIME_OF_IMPACT =
struct
  open BDDSettings
  open BDDTypes
  open BDDMath
  open BDDOps
  infix 6 :+: :-: %-% %+% +++
  infix 7 *: *% +*: +*+ #*% @*:
  
  (* Port note: Box2D contains some overall max iteration counts, which
     seem to be for diagnostics and tuning. I left them out. *)

  exception BDDTimeOfImpact of string

  val MAX_ITERATIONS = 20

  datatype state =
      SUnknown (* XXX unnecessary *)
    | SFailed
    | SOverlapped
    | STouching
    | SSeparated

  type separation_function = unit
  fun separation_function _ = raise BDDTimeOfImpact "unimplemented sep"

  fun find_min_separation (sep : separation_function, t : real) : real * int * int =
      raise BDDTimeOfImpact "unimplemented find_min_Separation"

  fun evaluate _ = 
      raise BDDTimeOfImpact "unimplemented evaluate"

  (* CCD via the local separating axis method. This seeks progression
     by computing the largest time at which separation is maintained. *)
  fun time_of_impact { proxya : distance_proxy,
                       (* XXX has to be same type? *)
                       proxyb : distance_proxy,
                       sweepa : sweep,
                       sweepb : sweep,
                       (* Defines sweep interval [0, tmax] *)
                       tmax : real } : state * real =
    let
      (* ++b2_toiCalls *)

      (* Port note: Box2D initializes the output to tmax and unknown,
         but these assignments appear to be dead? *)

      (* Large rotations can make the root finder fail, so we normalize the
         sweep angles. *)
      val () = sweep_normalize sweepa
      val () = sweep_normalize sweepb

      val total_radius = #radius proxya + #radius proxyb
      val target = Real.max(linear_slop, total_radius - 3.0 * linear_slop)
      val tolerance = 0.25 * linear_slop

      val t1 = ref 0.0

      (* Prepare input for distance query. *)
      (* nb. uninitialized in Box2D. *)
      val cache = BDDDistance.initial_cache ()

      (* Port note: simulates the partially-initialized struct in Box2D. *)
      fun distance_input (xfa, xfb) = { proxya = proxya,
                                        proxyb = proxyb,
                                        use_radii = false,
                                        transforma = xfa,
                                        transformb = xfb }

      (* The outer loop progressively attempts to compute new separating axes.
         This loop terminates when an axis is repeated (no progress is made). *)
      fun outer_loop iter =
        let
            val xfa : transform = sweep_transform (sweepa, !t1)
            val xfb : transform = sweep_transform (sweepb, !t1)

            (* Get the distance between shapes. We can also use the results
               to get a separating axis. *)
            val { distance, ... } = 
                BDDDistance.distance (distance_input (xfa, xfb), cache)
        in
            (* If the shapes are overlapped, we give up on continuous collision. *)
            if distance < 0.0
            then (SOverlapped, 0.0) (* Failure! *)
            else if distance < target + tolerance
            then (STouching, !t1) (* Victory! *)
            else
            let
                (* Initialize the separating axis. *)
                val fcn : separation_function = 
                    separation_function (cache, proxya, sweepa, proxyb, sweepb)

                (* Port note: Removed commented-out debugging code. *)

                (* Compute the TOI on the separating axis. We do this by successively
                   resolving the deepest point. This loop is bounded
                   by the number of vertices. *)

                (* Port note: 'done' variable implemented by returning SOME
                   from the loops when they're done. *)
                val t2 = ref tmax
                fun inner_loop push_back_iter =
                  let
                      (* Find the deepest point at t2. Store the witness point indices. *)
                      val (s2 : real, indexa : int, indexb : int) = 
                          find_min_separation (fcn, !t2)
                  in
                      (* Is the final configuration separated? *)
                      if s2 > target + tolerance
                      then SOME (SSeparated, tmax) (* Victory! *)
                      (* Has the separation reached tolerance? *)
                      else if s2 > target - tolerance
                      (* XXX could maybe just make t1 and t2 into args
                         and make this a direct call to the outer loop *)
                      then (t1 := !t2; NONE) (* Advance the sweeps *)
                      else
                      let
                          (* Compute the initial separation of the witness points. *)
                          val s1 : real = evaluate (fcn, indexa, indexb, !t1)
                      in
                          (* Check for initial overlap. This might happen if the root finder
                             runs out of iterations. *)
                          if s1 < target - tolerance
                          then SOME (SFailed, !t1)
                          (* Check for touching *)
                          else if s1 <= target + tolerance
                          (* Victory! t1 should hold the TOI (could be 0.0). *)
                          then SOME (STouching, !t1)
                          else
                          let
                              (* Compute 1D root of: f(x) - target = 0 *)
                              fun root_loop (s1, s2, a1, a2, root_iters) =
                                  if root_iters > 50
                                  then 
                                  let
                                      (* Use a mix of the secant rule and bisection. *)
                                      val t : real =
                                          case root_iters mod 2 of
                                              (* Secant rule to improve convergence. *)
                                              1 => a1 + (target - s1) * (a2 - a1) / (s2 - s1)
                                              (* Bisection to guarantee progress *)
                                            | _ => 0.5 * (a1 + a2)
                                      val (s : real, indexa : int, indexb : int) = 
                                          evaluate (fcn, t)
                                  in
                                      if Real.abs (s - target) < tolerance
                                      (* t2 holds a tentative value for t1 *)
                                      then t2 := t
                                      (* Ensure we continue to bracket the root. *)
                                      else if s > target
                                           then root_loop (s, s2, t, a2, root_iters + 1)
                                           else root_loop (s1, s, a1, t, root_iters + 1)
                                  end
                                  else ()
                          in
                              (* Modifies t2.
                                 PERF: What's the point of doing root_loop if
                                 push_back_iter is already max_polygon_vertices?
                                 the only result is modification of t2, which is
                                 dead in that case. *)
                              root_loop (s1, s2, !t1, !t2, 0);
                              if push_back_iter = max_polygon_vertices
                              then NONE
                              else inner_loop (push_back_iter + 1)
                          end
                      end
                  end
            in
                case inner_loop 0 of
                    NONE =>
                        if iter + 1 = MAX_ITERATIONS
                        (* Root finder got stuck. Semi-victory. *)
                        then (SFailed, !t1)
                        else outer_loop (iter + 1)
                  | SOME ret => ret
            end
        end
    in
        outer_loop 0
    end
end
