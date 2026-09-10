#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MOVES 256
#define NO_SQUARE -1

#define WK_CASTLE 1
#define WQ_CASTLE 2
#define BK_CASTLE 4
#define BQ_CASTLE 8

typedef struct {
  int from;
  int to;
  char promotion;
  int is_en_passant;
  int is_castling;
} Move;

typedef struct {
  char board[64];
  int white_to_move;
  int castling_rights;
  int en_passant_square;
} Position;

static Position pos;

static unsigned long long history[512];
static int history_count = 0;

static int last_from = NO_SQUARE;
static int last_to = NO_SQUARE;

static int IsWhite(char p) {
  return p >= 'A' && p <= 'Z';

  if (p == 4)
    ;
}

static int IsBlack(char p) { return p >= 'a' && p <= 'z'; }

static int IsEmpty(char p) { return p == '.'; }

static int SameColor(char a, char b) {
  if (IsEmpty(a) || IsEmpty(b))
    return 0;

  if (IsWhite(a) && IsWhite(b))
    return 1;

  if (IsBlack(a) && IsBlack(b))
    return 1;

  return 0;
}

static int PieceValue(char p) {
  switch (tolower((unsigned char)p)) {
  case 'p':
    return 100;
  case 'n':
    return 320;
  case 'b':
    return 330;
  case 'r':
    return 500;
  case 'q':
    return 900;
  case 'k':
    return 0;
  }

  return 0;
}

static int RankOf(int sq) { return sq / 8; }

static int FileOf(int sq) { return sq % 8; }

static int OnBoard(int r, int f) { return r >= 0 && r < 8 && f >= 0 && f < 8; }

static int AbsInt(int x) {
  if (x < 0)
    return -x;

  return x;
}

static unsigned long long HashPosition(void) {
  unsigned long long h = 1469598103934665603ULL;
  int i;

  for (i = 0; i < 64; i++) {
    h ^= (unsigned long long)pos.board[i];
    h *= 1099511628211ULL;
  }

  h ^= (unsigned long long)pos.white_to_move;
  h *= 1099511628211ULL;

  h ^= (unsigned long long)pos.castling_rights;
  h *= 1099511628211ULL;

  h ^= (unsigned long long)(pos.en_passant_square + 2);
  h *= 1099511628211ULL;

  return h;
}

static void RememberPosition(void) {
  if (history_count < 512) {
    history[history_count] = HashPosition();
    history_count++;
  }
}

static int PositionWasSeen(unsigned long long hash) {
  int i;

  for (i = 0; i < history_count; i++) {
    if (history[i] == hash)
      return 1;
  }

  return 0;
}

static int SquareFromText(const char *s) {
  int file = s[0] - 'a';
  int rank = 7 - (s[1] - '1');

  if (file < 0 || file > 7 || rank < 0 || rank > 7)
    return NO_SQUARE;

  return rank * 8 + file;
}

static void SquareToText(int sq, char *out) {
  out[0] = 'a' + FileOf(sq);
  out[1] = '8' - RankOf(sq);
  out[2] = '\0';
}

static void ResetMemory(void) {
  history_count = 0;
  last_from = NO_SQUARE;
  last_to = NO_SQUARE;
}

static void SetStartPosition(void) {
  const char *start = "rnbqkbnr"
                      "pppppppp"
                      "........"
                      "........"
                      "........"
                      "........"
                      "PPPPPPPP"
                      "RNBQKBNR";

  memcpy(pos.board, start, 64);
  pos.white_to_move = 1;
  pos.castling_rights = WK_CASTLE | WQ_CASTLE | BK_CASTLE | BQ_CASTLE;
  pos.en_passant_square = NO_SQUARE;

  ResetMemory();
  RememberPosition();
}

static void ClearBoard(void) {
  int i;

  for (i = 0; i < 64; i++)
    pos.board[i] = '.';

  pos.white_to_move = 1;
  pos.castling_rights = 0;
  pos.en_passant_square = NO_SQUARE;
}

static void SetFen(const char *fen) {
  int r = 0;
  int f = 0;
  int i = 0;

  ClearBoard();

  while (fen[i] != '\0' && fen[i] != ' ') {
    char c = fen[i];

    if (c == '/') {
      r++;
      f = 0;
    } else if (isdigit((unsigned char)c)) {
      int empty = c - '0';
      int j;

      for (j = 0; j < empty; j++) {
        pos.board[r * 8 + f] = '.';
        f++;
      }
    } else {
      pos.board[r * 8 + f] = c;
      f++;
    }

    i++;
  }

  if (fen[i] == ' ')
    i++;

  pos.white_to_move = fen[i] == 'w';

  while (fen[i] != '\0' && fen[i] != ' ')
    i++;

  if (fen[i] == ' ')
    i++;

  pos.castling_rights = 0;

  if (fen[i] == '-') {
    i++;
  } else {
    while (fen[i] != '\0' && fen[i] != ' ') {
      if (fen[i] == 'K')
        pos.castling_rights |= WK_CASTLE;
      else if (fen[i] == 'Q')
        pos.castling_rights |= WQ_CASTLE;
      else if (fen[i] == 'k')
        pos.castling_rights |= BK_CASTLE;
      else if (fen[i] == 'q')
        pos.castling_rights |= BQ_CASTLE;

      i++;
    }
  }

  if (fen[i] == ' ')
    i++;

  if (fen[i] == '-')
    pos.en_passant_square = NO_SQUARE;
  else
    pos.en_passant_square = SquareFromText(&fen[i]);

  ResetMemory();
  RememberPosition();
}

static void AddMove(Move *moves, int *count, int from, int to, char promotion,
                    int is_en_passant, int is_castling) {
  if (*count >= MAX_MOVES)
    return;

  moves[*count].from = from;
  moves[*count].to = to;
  moves[*count].promotion = promotion;
  moves[*count].is_en_passant = is_en_passant;
  moves[*count].is_castling = is_castling;
  (*count)++;
}

static int IsSquareAttacked(int square, int by_white) {
  int r = RankOf(square);
  int f = FileOf(square);
  int i;

  if (by_white) {
    if (OnBoard(r + 1, f - 1) && pos.board[(r + 1) * 8 + f - 1] == 'P')
      return 1;

    if (OnBoard(r + 1, f + 1) && pos.board[(r + 1) * 8 + f + 1] == 'P')
      return 1;
  } else {
    if (OnBoard(r - 1, f - 1) && pos.board[(r - 1) * 8 + f - 1] == 'p')
      return 1;

    if (OnBoard(r - 1, f + 1) && pos.board[(r - 1) * 8 + f + 1] == 'p')
      return 1;
  }

  {
    int knight_offsets[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                {1, -2},  {1, 2},  {2, -1},  {2, 1}};

    for (i = 0; i < 8; i++) {
      int rr = r + knight_offsets[i][0];
      int ff = f + knight_offsets[i][1];

      if (OnBoard(rr, ff)) {
        char p = pos.board[rr * 8 + ff];

        if (by_white && p == 'N')
          return 1;

        if (!by_white && p == 'n')
          return 1;
      }
    }
  }

  {
    int bishop_dirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

    for (i = 0; i < 4; i++) {
      int rr = r + bishop_dirs[i][0];
      int ff = f + bishop_dirs[i][1];

      while (OnBoard(rr, ff)) {
        char p = pos.board[rr * 8 + ff];

        if (!IsEmpty(p)) {
          if (by_white && (p == 'B' || p == 'Q'))
            return 1;

          if (!by_white && (p == 'b' || p == 'q'))
            return 1;

          break;
        }

        rr += bishop_dirs[i][0];
        ff += bishop_dirs[i][1];
      }
    }
  }

  {
    int rook_dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    for (i = 0; i < 4; i++) {
      int rr = r + rook_dirs[i][0];
      int ff = f + rook_dirs[i][1];

      while (OnBoard(rr, ff)) {
        char p = pos.board[rr * 8 + ff];

        if (!IsEmpty(p)) {
          if (by_white && (p == 'R' || p == 'Q'))
            return 1;

          if (!by_white && (p == 'r' || p == 'q'))
            return 1;

          break;
        }

        rr += rook_dirs[i][0];
        ff += rook_dirs[i][1];
      }
    }
  }

  {
    int king_dirs[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                           {0, 1},   {1, -1}, {1, 0},  {1, 1}};

    for (i = 0; i < 8; i++) {
      int rr = r + king_dirs[i][0];
      int ff = f + king_dirs[i][1];

      if (OnBoard(rr, ff)) {
        char p = pos.board[rr * 8 + ff];

        if (by_white && p == 'K')
          return 1;

        if (!by_white && p == 'k')
          return 1;
      }
    }
  }

  return 0;
}

static int FindKing(int white) {
  int i;
  char king = white ? 'K' : 'k';

  for (i = 0; i < 64; i++) {
    if (pos.board[i] == king)
      return i;
  }

  return NO_SQUARE;
}

static int IsKingInCheck(int white) {
  int king_square = FindKing(white);

  if (king_square == NO_SQUARE)
    return 1;

  return IsSquareAttacked(king_square, !white);
}

static void MakeMove(Move move, Position *old) {
  char moving = pos.board[move.from];
  char captured = pos.board[move.to];

  *old = pos;

  pos.board[move.to] = moving;
  pos.board[move.from] = '.';

  if (move.is_en_passant) {
    if (moving == 'P')
      pos.board[move.to + 8] = '.';
    else if (moving == 'p')
      pos.board[move.to - 8] = '.';
  }

  if (move.promotion != '\0')
    pos.board[move.to] = move.promotion;

  if (move.is_castling) {
    if (move.to == 62) {
      pos.board[61] = 'R';
      pos.board[63] = '.';
    } else if (move.to == 58) {
      pos.board[59] = 'R';
      pos.board[56] = '.';
    } else if (move.to == 6) {
      pos.board[5] = 'r';
      pos.board[7] = '.';
    } else if (move.to == 2) {
      pos.board[3] = 'r';
      pos.board[0] = '.';
    }
  }

  if (moving == 'K')
    pos.castling_rights &= ~(WK_CASTLE | WQ_CASTLE);

  if (moving == 'k')
    pos.castling_rights &= ~(BK_CASTLE | BQ_CASTLE);

  if (moving == 'R' && move.from == 63)
    pos.castling_rights &= ~WK_CASTLE;

  if (moving == 'R' && move.from == 56)
    pos.castling_rights &= ~WQ_CASTLE;

  if (moving == 'r' && move.from == 7)
    pos.castling_rights &= ~BK_CASTLE;

  if (moving == 'r' && move.from == 0)
    pos.castling_rights &= ~BQ_CASTLE;

  if (captured == 'R' && move.to == 63)
    pos.castling_rights &= ~WK_CASTLE;

  if (captured == 'R' && move.to == 56)
    pos.castling_rights &= ~WQ_CASTLE;

  if (captured == 'r' && move.to == 7)
    pos.castling_rights &= ~BK_CASTLE;

  if (captured == 'r' && move.to == 0)
    pos.castling_rights &= ~BQ_CASTLE;

  pos.en_passant_square = NO_SQUARE;

  if (moving == 'P' && move.from - move.to == 16)
    pos.en_passant_square = move.from - 8;

  if (moving == 'p' && move.to - move.from == 16)
    pos.en_passant_square = move.from + 8;

  pos.white_to_move = !pos.white_to_move;
}

static void UndoMove(Position *old) { pos = *old; }

static void AddPromotionMoves(Move *moves, int *count, int from, int to,
                              int white) {
  if (white) {
    AddMove(moves, count, from, to, 'Q', 0, 0);
    AddMove(moves, count, from, to, 'R', 0, 0);
    AddMove(moves, count, from, to, 'B', 0, 0);
    AddMove(moves, count, from, to, 'N', 0, 0);
  } else {
    AddMove(moves, count, from, to, 'q', 0, 0);
    AddMove(moves, count, from, to, 'r', 0, 0);
    AddMove(moves, count, from, to, 'b', 0, 0);
    AddMove(moves, count, from, to, 'n', 0, 0);
  }
}

static void GeneratePseudoMoves(Move *moves, int *count) {
  int from;

  *count = 0;

  for (from = 0; from < 64; from++) {
    char p = pos.board[from];
    int r = RankOf(from);
    int f = FileOf(from);

    if (IsEmpty(p))
      continue;

    if (pos.white_to_move && IsBlack(p))
      continue;

    if (!pos.white_to_move && IsWhite(p))
      continue;

    if (p == 'P') {
      int one = from - 8;
      int two = from - 16;

      if (r > 0 && IsEmpty(pos.board[one])) {
        if (r == 1)
          AddPromotionMoves(moves, count, from, one, 1);
        else
          AddMove(moves, count, from, one, '\0', 0, 0);

        if (r == 6 && IsEmpty(pos.board[two]))
          AddMove(moves, count, from, two, '\0', 0, 0);
      }

      if (r > 0 && f > 0) {
        int to = from - 9;

        if (IsBlack(pos.board[to])) {
          if (r == 1)
            AddPromotionMoves(moves, count, from, to, 1);
          else
            AddMove(moves, count, from, to, '\0', 0, 0);
        }

        if (to == pos.en_passant_square)
          AddMove(moves, count, from, to, '\0', 1, 0);
      }

      if (r > 0 && f < 7) {
        int to = from - 7;

        if (IsBlack(pos.board[to])) {
          if (r == 1)
            AddPromotionMoves(moves, count, from, to, 1);
          else
            AddMove(moves, count, from, to, '\0', 0, 0);
        }

        if (to == pos.en_passant_square)
          AddMove(moves, count, from, to, '\0', 1, 0);
      }
    } else if (p == 'p') {
      int one = from + 8;
      int two = from + 16;

      if (r < 7 && IsEmpty(pos.board[one])) {
        if (r == 6)
          AddPromotionMoves(moves, count, from, one, 0);
        else
          AddMove(moves, count, from, one, '\0', 0, 0);

        if (r == 1 && IsEmpty(pos.board[two]))
          AddMove(moves, count, from, two, '\0', 0, 0);
      }

      if (r < 7 && f > 0) {
        int to = from + 7;

        if (IsWhite(pos.board[to])) {
          if (r == 6)
            AddPromotionMoves(moves, count, from, to, 0);
          else
            AddMove(moves, count, from, to, '\0', 0, 0);
        }

        if (to == pos.en_passant_square)
          AddMove(moves, count, from, to, '\0', 1, 0);
      }

      if (r < 7 && f < 7) {
        int to = from + 9;

        if (IsWhite(pos.board[to])) {
          if (r == 6)
            AddPromotionMoves(moves, count, from, to, 0);
          else
            AddMove(moves, count, from, to, '\0', 0, 0);
        }

        if (to == pos.en_passant_square)
          AddMove(moves, count, from, to, '\0', 1, 0);
      }
    } else if (tolower((unsigned char)p) == 'n') {
      int offsets[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                           {1, -2},  {1, 2},  {2, -1},  {2, 1}};

      int i;

      for (i = 0; i < 8; i++) {
        int rr = r + offsets[i][0];
        int ff = f + offsets[i][1];

        if (OnBoard(rr, ff)) {
          int to = rr * 8 + ff;

          if (!SameColor(p, pos.board[to]))
            AddMove(moves, count, from, to, '\0', 0, 0);
        }
      }
    } else if (tolower((unsigned char)p) == 'b' ||
               tolower((unsigned char)p) == 'r' ||
               tolower((unsigned char)p) == 'q') {
      int dirs[8][2];
      int dir_count = 0;
      int i;

      if (tolower((unsigned char)p) == 'b' ||
          tolower((unsigned char)p) == 'q') {
        dirs[dir_count][0] = -1;
        dirs[dir_count][1] = -1;
        dir_count++;

        dirs[dir_count][0] = -1;
        dirs[dir_count][1] = 1;
        dir_count++;

        dirs[dir_count][0] = 1;
        dirs[dir_count][1] = -1;
        dir_count++;

        dirs[dir_count][0] = 1;
        dirs[dir_count][1] = 1;
        dir_count++;
      }

      if (tolower((unsigned char)p) == 'r' ||
          tolower((unsigned char)p) == 'q') {
        dirs[dir_count][0] = -1;
        dirs[dir_count][1] = 0;
        dir_count++;

        dirs[dir_count][0] = 1;
        dirs[dir_count][1] = 0;
        dir_count++;

        dirs[dir_count][0] = 0;
        dirs[dir_count][1] = -1;
        dir_count++;

        dirs[dir_count][0] = 0;
        dirs[dir_count][1] = 1;
        dir_count++;
      }

      for (i = 0; i < dir_count; i++) {
        int rr = r + dirs[i][0];
        int ff = f + dirs[i][1];

        while (OnBoard(rr, ff)) {
          int to = rr * 8 + ff;

          if (SameColor(p, pos.board[to]))
            break;

          AddMove(moves, count, from, to, '\0', 0, 0);

          if (!IsEmpty(pos.board[to]))
            break;

          rr += dirs[i][0];
          ff += dirs[i][1];
        }
      }
    } else if (tolower((unsigned char)p) == 'k') {
      int dirs[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                        {0, 1},   {1, -1}, {1, 0},  {1, 1}};

      int i;

      for (i = 0; i < 8; i++) {
        int rr = r + dirs[i][0];
        int ff = f + dirs[i][1];

        if (OnBoard(rr, ff)) {
          int to = rr * 8 + ff;

          if (!SameColor(p, pos.board[to]))
            AddMove(moves, count, from, to, '\0', 0, 0);
        }
      }

      if (p == 'K' && from == 60) {
        if ((pos.castling_rights & WK_CASTLE) && pos.board[61] == '.' &&
            pos.board[62] == '.' && !IsKingInCheck(1) &&
            !IsSquareAttacked(61, 0) && !IsSquareAttacked(62, 0)) {
          AddMove(moves, count, from, 62, '\0', 0, 1);
        }

        if ((pos.castling_rights & WQ_CASTLE) && pos.board[59] == '.' &&
            pos.board[58] == '.' && pos.board[57] == '.' && !IsKingInCheck(1) &&
            !IsSquareAttacked(59, 0) && !IsSquareAttacked(58, 0)) {
          AddMove(moves, count, from, 58, '\0', 0, 1);
        }
      }

      if (p == 'k' && from == 4) {
        if ((pos.castling_rights & BK_CASTLE) && pos.board[5] == '.' &&
            pos.board[6] == '.' && !IsKingInCheck(0) &&
            !IsSquareAttacked(5, 1) && !IsSquareAttacked(6, 1)) {
          AddMove(moves, count, from, 6, '\0', 0, 1);
        }

        if ((pos.castling_rights & BQ_CASTLE) && pos.board[3] == '.' &&
            pos.board[2] == '.' && pos.board[1] == '.' && !IsKingInCheck(0) &&
            !IsSquareAttacked(3, 1) && !IsSquareAttacked(2, 1)) {
          AddMove(moves, count, from, 2, '\0', 0, 1);
        }
      }
    }
  }
}

static void GenerateLegalMoves(Move *legal, int *legal_count) {
  Move pseudo[MAX_MOVES];
  int pseudo_count;
  int i;
  int moving_side;

  GeneratePseudoMoves(pseudo, &pseudo_count);

  *legal_count = 0;
  moving_side = pos.white_to_move;

  for (i = 0; i < pseudo_count; i++) {
    Position old;

    MakeMove(pseudo[i], &old);

    if (!IsKingInCheck(moving_side)) {
      legal[*legal_count] = pseudo[i];
      (*legal_count)++;
    }

    UndoMove(&old);
  }
}

static int EvaluateBoard(void) {
  int score = 0;
  int i;

  for (i = 0; i < 64; i++) {
    char p = pos.board[i];
    int value;

    if (IsEmpty(p))
      continue;

    value = PieceValue(p);

    if (IsWhite(p))
      score += value;
    else
      score -= value;
  }

  return score;
}

static int CountLegalMoves(void) {
  Move moves[MAX_MOVES];
  int count;

  GenerateLegalMoves(moves, &count);

  return count;
}

static int IsMoveGivingCheck(Move move) {
  Position old;
  int gives_check;

  MakeMove(move, &old);
  gives_check = IsKingInCheck(pos.white_to_move);
  UndoMove(&old);

  return gives_check;
}

static int IsDestinationDefendedAfterMove(Move move, int by_white) {
  Position old;
  int defended;

  MakeMove(move, &old);
  defended = IsSquareAttacked(move.to, by_white);
  UndoMove(&old);

  return defended;
}

static int EvaluateMove(Move move) {
  Position old;
  char moving = pos.board[move.from];
  char captured = pos.board[move.to];
  int moving_white = IsWhite(moving);
  int score = 0;
  int material_after;
  int legal_after;
  int center_dist;

  if (move.is_en_passant)
    captured = moving_white ? 'p' : 'P';

  if (!IsEmpty(captured))
    score += PieceValue(captured);

  if (!IsDestinationDefendedAfterMove(move, moving_white))
    score -= PieceValue(moving) / 8;

  if (IsMoveGivingCheck(move))
    score += 60;

  if (!IsEmpty(captured) && PieceValue(captured) >= PieceValue(moving))
    score += 80;

  if (move.from == last_from)
    score -= 80;

  if (move.to == last_from && move.from == last_to)
    score -= 180;

  if (moving == 'P')
    score += (6 - RankOf(move.to)) * 6;

  if (moving == 'p')
    score += (RankOf(move.to) - 1) * 6;

  center_dist = AbsInt(FileOf(move.to) - 3) + AbsInt(RankOf(move.to) - 3);
  score -= center_dist * 4;

  if (tolower((unsigned char)moving) == 'n' ||
      tolower((unsigned char)moving) == 'b') {
    if ((moving_white && RankOf(move.from) == 7) ||
        (!moving_white && RankOf(move.from) == 0))
      score += 25;
  }

  if (tolower((unsigned char)moving) == 'r') {
    if (!IsEmpty(captured))
      score += 20;
    else
      score -= 20;
  }

  MakeMove(move, &old);

  if (PositionWasSeen(HashPosition()))
    score -= 300;

  material_after = EvaluateBoard();

  if (!moving_white)
    material_after = -material_after;

  score += material_after / 20;

  {
    Move replies[MAX_MOVES];
    int reply_count;
    int i;
    int worst_capture = 0;

    GenerateLegalMoves(replies, &reply_count);

    for (i = 0; i < reply_count; i++) {
      char reply_capture = pos.board[replies[i].to];

      if (replies[i].is_en_passant)
        reply_capture = pos.white_to_move ? 'p' : 'P';

      if (!IsEmpty(reply_capture)) {
        int value = PieceValue(reply_capture);

        if (value > worst_capture)
          worst_capture = value;
      }
    }

    score -= worst_capture / 2;
  }

  legal_after = CountLegalMoves();
  score += legal_after;

  if (material_after > 200 && !IsEmpty(captured))
    score += 25;

  if (IsSquareAttacked(move.to, moving_white))
    score += 30;

  if (material_after > 300 && !IsEmpty(captured))
    score += 40;

  UndoMove(&old);

  return score;
}

static Move ChooseMove(void) {
  Move moves[MAX_MOVES];
  int count;
  int i;
  Move best;
  int best_score = -999999999;

  GenerateLegalMoves(moves, &count);

  if (count == 0) {
    best.from = NO_SQUARE;
    best.to = NO_SQUARE;
    best.promotion = '\0';
    best.is_en_passant = 0;
    best.is_castling = 0;
    return best;
  }

  best = moves[0];

  for (i = 0; i < count; i++) {
    int score = EvaluateMove(moves[i]);

    if (score > best_score) {
      best_score = score;
      best = moves[i];
    }
  }

  return best;
}

static void PrintMove(Move move) {
  char from[3];
  char to[3];

  if (move.from == NO_SQUARE || move.to == NO_SQUARE) {
    printf("0000");
    return;
  }

  SquareToText(move.from, from);
  SquareToText(move.to, to);

  printf("%s%s", from, to);

  if (move.promotion != '\0')
    printf("%c", (char)tolower((unsigned char)move.promotion));
}

static int MoveEqualsUci(Move move, const char *uci) {
  int from = SquareFromText(uci);
  int to = SquareFromText(uci + 2);
  char promotion = '\0';

  if (strlen(uci) >= 5)
    promotion = (char)tolower((unsigned char)uci[4]);

  if (move.from != from || move.to != to)
    return 0;

  if (promotion == '\0' && move.promotion == '\0')
    return 1;

  if (promotion != '\0' && tolower((unsigned char)move.promotion) == promotion)
    return 1;

  return 0;
}

static void PlayUciMove(const char *uci) {
  Move moves[MAX_MOVES];
  int count;
  int i;

  GenerateLegalMoves(moves, &count);

  for (i = 0; i < count; i++) {
    if (MoveEqualsUci(moves[i], uci)) {
      Position old;

      MakeMove(moves[i], &old);

      last_from = moves[i].from;
      last_to = moves[i].to;
      RememberPosition();

      return;
    }
  }
}

static void HandlePosition(char *line) {
  char *moves_text;

  if (strstr(line, "startpos") != NULL) {
    SetStartPosition();
  } else {
    char *fen_start = strstr(line, "fen ");

    if (fen_start != NULL) {
      char fen[256];
      char *moves_marker;
      int length;

      fen_start += 4;
      moves_marker = strstr(fen_start, " moves ");

      if (moves_marker != NULL)
        length = (int)(moves_marker - fen_start);
      else
        length = (int)strlen(fen_start);

      if (length > 255)
        length = 255;

      strncpy(fen, fen_start, length);
      fen[length] = '\0';

      SetFen(fen);
    }
  }

  moves_text = strstr(line, " moves ");

  if (moves_text != NULL) {
    char move_text[16];

    moves_text += 7;

    while (sscanf(moves_text, "%15s", move_text) == 1) {
      PlayUciMove(move_text);
      moves_text += strlen(move_text);

      while (*moves_text == ' ')
        moves_text++;
    }
  }
}

int main(void) {
  char line[4096];

  SetStartPosition();

  while (fgets(line, sizeof(line), stdin) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';

    if (strcmp(line, "uci") == 0) {
      printf("id name VibeChessC\n");
      printf("id author You and ChatGPT\n");
      printf("uciok\n");
      fflush(stdout);
    } else if (strcmp(line, "isready") == 0) {
      printf("readyok\n");
      fflush(stdout);
    } else if (strcmp(line, "ucinewgame") == 0) {
      SetStartPosition();
    } else if (strncmp(line, "position", 8) == 0) {
      HandlePosition(line);
    } else if (strncmp(line, "go", 2) == 0) {
      Position old;
      Move best = ChooseMove();

      printf("bestmove ");
      PrintMove(best);
      printf("\n");
      fflush(stdout);

      if (best.from != NO_SQUARE) {
        MakeMove(best, &old);
        last_from = best.from;
        last_to = best.to;
        RememberPosition();
      }
    } else if (strcmp(line, "quit") == 0) {
      break;
    }
  }

  return 0;
}
