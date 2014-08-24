
var DEBUG = false; // true;

var WIDTH = 320;
var HEIGHT = 200;
var PX = 3;
var MINFRAMEMS = 25.0;

var TILESIZE = 20;

var TILESW = 12;
var TILESH = 8;

var BOARDSTARTX = 68;
var BOARDSTARTY = 20;

// Order is assumed.
var UP = 0;
var DOWN = 1;
var LEFT = 2;
var RIGHT = 3;
function ReverseDir(d) {
  switch (d) {
    case UP: return DOWN;
    case DOWN: return UP;
    case LEFT: return RIGHT;
    case RIGHT: return LEFT;
  }
  return undefined;
}

var INDICATOR_IN = 1;
var INDICATOR_OUT = 2;
var INDICATOR_BOTH = 3;

var TRAYX = 6;
var TRAYY = 12;
var TRAYW = 51;
var TRAYH = 138;

var TRAYPLACEX = 11;
var TRAYPLACEY = 25;

var CELL_HEAD = 1;
var CELL_WIRE = 2;
// etc.?

var WIRE_NS = 1;
var WIRE_WE = 2;
var WIRE_SE = 3;
var WIRE_SW = 4;
var WIRE_NE = 5;
var WIRE_NW = 6;

// goal box
var GOALX = 6;
var GOALY = 153;
var GOALW = 51;
var GOALH = 37;

// where the goal piece goes
var GOALPLACEX = 11;
var GOALPLACEY = 164;

var FONTW = 9;
var FONTH = 16;
var FONTOVERLAP = 1;
var FONTCHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ" +
    "abcdefghijklmnopqrstuvwxyz0123456789`-=" +
    "[]\\;',./~!@#$%^&*()_+{}|:\"<>?";
