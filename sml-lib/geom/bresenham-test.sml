structure BresenhamTest =
struct

  exception BresenhamTest of string

  fun ctos (x, y) = Int.toString x ^ "," ^ Int.toString y
  fun pltos p = StringUtil.delimit "  " (map ctos p)

  fun expect (st, en) expected =
    let
      val actual = Bresenham.points st en
      val len = Bresenham.num_points st en
    in
      if len = length actual
      then ()
      else raise BresenhamTest ("For " ^ ctos st ^ " -> " ^
                                ctos en ^ "\nWrong num_points; got " ^
                                Int.toString len ^ "\nBut actually: " ^
                                pltos actual ^ "\n");

      if actual = expected
      then ()
      else raise BresenhamTest ("For " ^ ctos st ^ " -> " ^
                                ctos en ^ "\nGot:\n" ^
                                pltos actual ^
                                "\nBut expected:\n" ^
                                pltos expected ^
                                "\n")
    end

  fun go () =
    let
    in
      expect ((0, 0), (1, 1)) [(0, 0), (1, 1)];
      expect ((0, 0), (2, 2)) [(0, 0), (1, 1), (2, 2)];
      expect ((0, 0), (0, 2)) [(0, 0), (0, 1), (0, 2)];
      expect ((0, 0), (0, 3)) [(0, 0), (0, 1), (0, 2), (0, 3)];
      expect ((0, 0), (0, ~3)) [(0, 0), (0, ~1), (0, ~2), (0, ~3)];
      expect ((0, 0), (~3, 1)) [(0, 0), (~1, 0), (~2, 1), (~3, 1)];
      expect ((~3, 1), (0, 0)) (rev [(0, 0), (~1, 0), (~2, 1), (~3, 1)]);
      expect ((1, 1), (2, 2)) [(1, 1), (2, 2)];
      expect ((2, 2), (1, 1)) [(2, 2), (1, 1)];
      expect ((2, 3), (1, 2)) [(2, 3), (1, 2)];
      expect ((1, 2), (3, 2)) [(1, 2), (2, 2), (3, 2)];
      expect ((1, 2), (~5, 3)) [(1, 2), (0, 2), (~1, 2), (~2, 3),
                                (~3, 3), (~4, 3), (~5, 3)]
    end
  handle BresenhamTest s =>
    let in
      print "FAILED:\n";
      print s;
      (* OS.Process.exit OS.Process.failure *)
      raise (BresenhamTest s)
    end
end

val () = BresenhamTest.go ()
