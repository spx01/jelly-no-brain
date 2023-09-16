const FUNCLIST = [
  ["GAME_test", null, ["number"]],
  ["GAME_new", "number", []],
  ["GAME_free", null, ["number"]],
  ["GAME_undo", "boolean", ["number"]],
  ["GAME_move_piece", "boolean", ["number", "number", "number", "number"]],
  ["GAME_get_cell", "number", ["number", "number", "number"]],
  ["GAME_get_move_count", "number", ["number"]],
  ["GAME_get_action_count", "number", ["number"]],
  ["GAME_get_undo_avail", "number", ["number"]],
  ["GAME_get_cell_type", "number", ["number", "number"]],
  ["GAME_get_color", "number", ["number", "number"]],
  ["GAME_piece_where_can_connect", "number", ["number", "number"]],
  ["GAME_get_block", "number", ["number", "number"]],
  ["GAME_block_is_fixed", "number", ["number", "number"]],
  ["GAME_get_cell_coords", "number", ["number", "number"]],
  ["GAME_print_current_state", null, ["number"]],
  ["GAME_get_current_state_block_count", "number", ["number"]],
  ["GAME_cell_where_connected", "number", ["number", "number"]],
  ["GAME_get_current_state_b64", "string", ["number"]],
];

// TODO: decorations

const BOARD_WIDTH = 14;
const BOARD_HEIGHT = 10;
const CELL_SIZE = 64;

const CORNER_RADIUS = 6;

let color_table = [
  ["#090710", 0],
  ["#8c8c8c", 1],
  ["#4f3718", 2],
  ["#22614f", 3],
  ["#66c360", 4],
  ["#130e3a", 5],
  ["#4b4b87", 6],
  ["#c962dd", 7],
  ["#e6e86a", 8],
  ["#e33b3b", 9],
];

const NOCOLOR_IDX = 0;
const WALL_IDX = 1;

const TILE_SIZE = 192;
const ATLAS_WIDTH = 1920;
const ATLAS_HEIGHT = 192;

let game = null;
let canvas = null;
let ctx = null;
const atlas = new Image();
let atlas_loaded = false;
let random_colors = sessionStorage.getItem("random_colors") == "true";
let cell_selected_pos = null;

function shuffle(array) {
  let currentIndex = array.length,
    randomIndex;

  // While there remain elements to shuffle.
  while (currentIndex > 0) {
    // Pick a remaining element.
    randomIndex = Math.floor(Math.random() * currentIndex);
    currentIndex--;

    // And swap it with the current element.
    [array[currentIndex], array[randomIndex]] = [
      array[randomIndex],
      array[currentIndex],
    ];
  }

  return array;
}

const CELL_TYPE = {
  EMPTY: 0,
  WALL: 1,
  PIECE: 2,
  EMERGE: 3,
};

const DIR = {
  LEFT: 0,
  RIGHT: 1,
  UP: 2,
  DOWN: 3,
};

const CON_DIR = {
  LEFT: 1 << DIR.LEFT,
  RIGHT: 1 << DIR.RIGHT,
  UP: 1 << DIR.UP,
  DOWN: 1 << DIR.DOWN,
};

function is_black_piece(cell) {
  const can_connect = GAME_piece_where_can_connect(game, cell);
  if (can_connect == -1 || can_connect == 0b1111) {
    return false;
  }
  return can_connect == GAME_cell_where_connected(game, cell);
}

function get_piece_color(cell) {
  if (is_black_piece(cell)) {
    return color_table[0][0];
  }
  // skip the wall and connective piece colors
  const color = GAME_get_color(game, cell) + 2;
  if (color >= color_table.length) {
    console.log("Color not implemented: " + color);
    return "#f0f0f0";
  }
  return color_table[color + 2][0];
}

function draw_wall_primitive(x, y) {
  ctx.lineWidth = 3;
  ctx.fillStyle = color_table[WALL_IDX][0];
  ctx.strokeRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

function draw_piece_primitive(x, y, cell) {
  ctx.fillStyle = get_piece_color(cell);
  ctx.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

function draw_game_primitive() {
  for (let y = 0; y < BOARD_HEIGHT; y++) {
    for (let x = 0; x < BOARD_WIDTH; x++) {
      const cell = GAME_get_cell(game, x, y);
      const cell_type = GAME_get_cell_type(game, cell);
      switch (cell_type) {
        case CELL_TYPE.EMPTY:
          break;
        case CELL_TYPE.WALL:
          draw_wall_primitive(x, y);
          break;
        case CELL_TYPE.EMERGE:
          draw_wall_primitive(x, y);
          break;
        case CELL_TYPE.PIECE:
          draw_piece_primitive(x, y, cell);
          break;
      }
    }
  }
}

function get_minitile_idxs(connected) {
  const res = [
    [null, null],
    [null, null],
  ];
  const full_border = [
    [0, 2],
    [6, 8],
  ];
  if (connected & CON_DIR.LEFT) {
    res[0][0] = res[0][0] ? 4 : 1;
    res[1][0] = res[1][0] ? 4 : 7;
  }
  if (connected & CON_DIR.RIGHT) {
    res[0][1] = res[0][1] ? 4 : 1;
    res[1][1] = res[1][1] ? 4 : 7;
  }
  if (connected & CON_DIR.UP) {
    res[0][0] = res[0][0] ? 4 : 3;
    res[0][1] = res[0][1] ? 4 : 5;
  }
  if (connected & CON_DIR.DOWN) {
    res[1][0] = res[1][0] ? 4 : 3;
    res[1][1] = res[1][1] ? 4 : 5;
  }
  for (let i = 0; i < 2; i++) {
    for (let j = 0; j < 2; j++) {
      res[i][j] = res[i][j] || full_border[i][j];
    }
  }
  return res;
}

function get_final_atlas_pos(macrotile_pos, minitile_idx, microtile_rel_pos) {
  const MICROTILE_SIZE = CELL_SIZE / 2;
  let minitile_pos = [];
  Object.assign(minitile_pos, macrotile_pos);
  minitile_pos[0] += ~~(minitile_idx % 3) * CELL_SIZE;
  minitile_pos[1] += ~~(minitile_idx / 3) * CELL_SIZE;
  return [
    (minitile_pos[0] + microtile_rel_pos[0] * MICROTILE_SIZE) | 0,
    (minitile_pos[1] + microtile_rel_pos[1] * MICROTILE_SIZE) | 0,
  ];
}

function draw_game() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  if (!atlas_loaded) {
    draw_game_primitive();
    return;
  }
  const atlas_pos_board = [];
  for (let y = 0; y < BOARD_HEIGHT; y++) {
    atlas_pos_board.push([]);
    for (let x = 0; x < BOARD_WIDTH; x++) {
      atlas_pos_board[y].push(get_tile_atlas_pos(GAME_get_cell(game, x, y)));
    }
  }

  const DELTA_DIR = [
    [-1, 0],
    [1, 0],
    [0, -1],
    [0, 1],
  ];

  for (let y = 0; y < BOARD_HEIGHT; y++) {
    for (let x = 0; x < BOARD_WIDTH; x++) {
      const cell = GAME_get_cell(game, x, y);
      const cell_type = GAME_get_cell_type(game, cell);
      if (cell_type == CELL_TYPE.EMPTY) {
        continue;
      }
      let connected = GAME_cell_where_connected(game, cell);
      // iterate in the directions so that we can make sure that the connected
      // pieces use the same atlas tile, otherwise don't consider them together
      // for micro tiling and use a decoration instead
      let border_dirs = 0;
      for (let dir = 0; dir < 4; dir++) {
        if (!(connected & (1 << dir))) {
          continue;
        }
        let new_pos = [x + DELTA_DIR[dir][0], y + DELTA_DIR[dir][1]];
        if (
          new_pos[0] < 0 ||
          new_pos[0] >= BOARD_WIDTH ||
          new_pos[1] < 0 ||
          new_pos[1] >= BOARD_HEIGHT
        ) {
          border_dirs |= 1 << dir;
          continue;
        }
        if (
          atlas_pos_board[y][x].toString() !=
          atlas_pos_board[new_pos[1]][new_pos[0]].toString()
        ) {
          border_dirs |= 1 << dir;
        }
      }
      connected ^= border_dirs;
      const minitile_idxs = get_minitile_idxs(connected);
      for (let i = 0; i < 2; i++) {
        for (let j = 0; j < 2; j++) {
          let pos = get_final_atlas_pos(
            atlas_pos_board[y][x],
            minitile_idxs[i][j],
            [j, i]
          );
          ctx.drawImage(
            atlas,
            pos[0],
            pos[1],
            CELL_SIZE / 2,
            CELL_SIZE / 2,
            (x + j / 2) * CELL_SIZE,
            (y + i / 2) * CELL_SIZE,
            CELL_SIZE / 2,
            CELL_SIZE / 2
          );
        }
      }
      // TODO: draw decorations
    }
  }

  if (cell_selected_pos) {
    const x = cell_selected_pos[0];
    const y = cell_selected_pos[1];
    ctx.lineWidth = 3;
    ctx.strokeStyle = "#32CD32";
    ctx.strokeRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
  }
}

function get_tile_atlas_pos(cell) {
  const type = GAME_get_cell_type(game, cell);
  if (type == CELL_TYPE.WALL || type == CELL_TYPE.EMERGE) {
    return [TILE_SIZE, 0];
  }
  if (is_black_piece(cell)) {
    return [0, 0];
  }
  const color = GAME_get_color(game, cell) + 2;
  const atlas_idx = color_table[color][1];
  return [atlas_idx * TILE_SIZE, 0];
}

function get_mouse_pos(evt) {
  const rect = canvas.getBoundingClientRect();
  const width = rect.right - rect.left;
  const height = rect.bottom - rect.top;
  return [
    ((evt.clientX - rect.left) / width) * BOARD_WIDTH * CELL_SIZE,
    ((evt.clientY - rect.top) / height) * BOARD_HEIGHT * CELL_SIZE,
  ];
}

function get_hovered_cell(evt) {
  const mouse_pos = get_mouse_pos(evt);
  const x = ~~(mouse_pos[0] / CELL_SIZE);
  const y = ~~(mouse_pos[1] / CELL_SIZE);
  return [x, y];
}

function click_handler(evt) {
  const pos = get_hovered_cell(evt);
  const cell = GAME_get_cell(game, pos[0], pos[1]);

  // cell_selected_pos holds the position of the cell that was selected
  // if the next click is to the letf or right of the selected cell, then
  // we move the piece in that direction
  // otherwise, we deselect the cell
  if (cell_selected_pos) {
    if (pos[0] == cell_selected_pos[0]) {
      if (pos[1] != cell_selected_pos[1]) {
        cell_selected_pos = null;
      }
      return;
    }

    const dir = pos[0] < cell_selected_pos[0] ? DIR.LEFT : DIR.RIGHT;
    GAME_move_piece(game, cell_selected_pos[0], cell_selected_pos[1], dir);
    cell_selected_pos = null;
  } else {
    if (GAME_get_cell_type(game, cell) != CELL_TYPE.PIECE) {
      return;
    }
    cell_selected_pos = pos;
  }

  draw_game();
}

function main() {
  FUNCLIST.forEach((func_def) => {
    window[func_def[0]] = Module.cwrap(...func_def);
  });

  canvas = document.getElementById("canvas");
  if (!canvas.getContext) {
    alert("Canvas not supported");
    return;
  }

  ctx = canvas.getContext("2d");

  ctx.canvas.height = (10 / 14) * ctx.canvas.width;
  canvas.width = CELL_SIZE * BOARD_WIDTH;
  canvas.height = CELL_SIZE * BOARD_HEIGHT;

  game = GAME_new();
  console.log(game);

  if (window.location.hash) {
    // init code in the future, from the url
    console.log("Loading from url: " + window.location.hash);
    const b64 = window.location.hash.slice(1);
    const data = atob(b64);
  }

  if (random_colors) {
    color_table = color_table.slice(0, 2).concat(shuffle(color_table.slice(2)));
  }

  draw_game();

  atlas.onload = function () {
    atlas_loaded = true;
    if (ctx) {
      draw_game();
    }
  };
  atlas.src = "assets/atlas.png";

  canvas.addEventListener("click", click_handler);
}

Module.onRuntimeInitialized = main;

function urlparams_to_obj(entries) {
  const res = {};
  for (const [key, value] of entries) {
    res[key] = value;
  }
  return res;
}
