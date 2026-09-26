# Composes the world map from blocks and checks it. Prints C string arrays for level.c.
W, H = 46, 46
G = [[' '] * W for _ in range(H)]   # ground
U = [[' '] * W for _ in range(H)]   # upper floor
B = [[' '] * W for _ in range(H)]   # basement (crypt)

def put(grid, x0, y0, rows):
    for dy, row in enumerate(rows):
        for dx, ch in enumerate(row):
            if ch != '?':
                grid[y0 + dy][x0 + dx] = ch

# ---- palace (north) 26 x 14, at x 10, y 1 ----
palace = [
 "WWWIWWWIWWWWIWWIWWWWIWWWIW",
 "WB..B.B#MMMMMHHMMMMM#.5..W",
 "W.......#Q.*MMMM*.Q#.....W",
 "W..2....#MRMMMMMMRM#..*..W",
 "WB.*...B#QMRM1MMRMQ#.....W",
 "W.......#MMRMMMMRMM#u...uW",
 "W...u...MM*RMMMMR*MM...4.W",
 "WB......#QMRMMMMRMQ#.....W",
 "W.......#MMRMMMMRMM#..e..W",
 "WBB..BB.#Q*RMMMMR*Q#u...uW",
 "WWWWWWWW#MMRMMMMRMM#WWWWWW",
 "W.u...u.#QMRMMMMRMQ#.u..uW",
 "W...*...MMMRMMMMRMMM..3..W",
 "WWWWWWWWWWWWWOWWWWWWWWWWWW",
]
# inside the palace, '.' and 'M' are both marble floor (the side rooms are plainer)
palace = [r.replace('.', 'm').replace('#', 'W') for r in palace]
put(G, 10, 1, palace)
# portico outside the palace door
put(G, 21, 15, ["Q::6Q"])

# ---- garden road between palace and ruins, y 16..19 ----
road = [
 "   yy  Y  e ::::::: e  Y  yy        ",
 "  Y  y   Y  :::::::  Y   y  Y       ",
 " y  Y  y    ::l::::    y  Y   y     ",
]
put(G, 8, 16, road)

# ---- the old ruins (south), 26 x 21 at x 10, y 19 ----
ruins = [
 "####%%#######D#######%%###",
 "#T.....h....#.....#......#",
 "#..P..P..P..#..r..#.^.C..#",
 "#...........D.....D.^..gT#",
 "#..P..P..P..#..C..#......#",
 "#T.....r....#..b..#v.f..c#",
 "####D########%%D####%%#D##",
 "%    ,    ,      ,       %",
 "%  p   s        s    p   %",
 "D    ,   ..........  ,   D",
 "%  .....f..A..F........  %",
 "%    ,   ..........      %",
 "%  p   s   S x  s    p   %",
 "%  ,    r  ,      ,  o   %",
 "####D#######%%###D########",
 "#....T.#b...........#...G#",
 "#.r....#..P.....P...#....#",
 "#...C..D....<r......L..l.#",
 "#v.....#..P.<...P.f.#....#",
 "#c..l..#...........T#T...#",
 "####%%#####%%#######%#####",
]
put(G, 10, 19, ruins)

# ---- west wing: overgrown garden cloister, x 1..9, y 19..33 ----
garden = [
 "%%%%#%%%%",
 "%yY y Yy%",
 "% ::::: %",
 "%Y:   :YD",
 "% : U : %",
 "%y:g   :%",
 "% :   :y%",
 "%Y:::::Y%",
 "%yy e yy%",
 "%  Y  j %",
 "%%%%D%%%%",
]
put(G, 1, 25, garden)

# ---- east wing: library (north) and flooded hall (south), x 36..44 ----
east = [
 "#########",
 "#B.B.B.T#",
 "#...q...#",
 "#B.l.B.g#",
 "#...q...#",
 "#B.B..B.#",
 "#T.....l#",
 "###D#####",
 "........%",
 "#####D###",
 "#wwwwwww#",
 "#wjwwwww#",
 "#wwwwwjw#",
 "#wwwwwww#",
 "#wwwjwww#",
 "#T.www.T#",
 "#########",
]
put(G, 36, 20, east)

# ---- upper floor: over the north-east ruin room, reached by the stairs '^' ----
# slabs only; parapets are generated along their edges. ' ' at column 30 = stairwell
upper = [
 "______",
 "_ C___",
 "_ _g__",
 "__l___",
 "______",
]
put(U, 29, 20, upper)

# ---- crypt, under the south ruins: stairs 'v' come down in the south-middle hall ----
crypt = [
 "   #########################   ",
 "   #Z.Z.Z#.......#Z.T..j..Z#   ",
 "   #.....#..z.z..#.........#   ",
 "   #..z..D...P...D.z..P..z.#   ",
 "   #T....#.......#.........#   ",
 "   ###D#########D#####D#####   ",
 "   #........#.....#.......T#   ",
 "   #.j..T...D..X..D..z..Z..#   ",
 "   #........#.....#........#   ",
 "   #########################   ",
]
put(B, 7, 34, crypt)

def show(g, name):
    print(f"static const char *{name}[] = {{")
    for row in g:
        print('    "' + ''.join(row) + '",')
    print("};")

# sanity: every door sits in a straight wall line
wallish = set('#%DLWIO')
for y in range(1, H - 1):
    for x in range(1, W - 1):
        if G[y][x] in 'DLO':
            h = G[y][x-1] in wallish and G[y][x+1] in wallish
            v = G[y-1][x] in wallish and G[y+1][x] in wallish
            if not (h or v):
                print("// WARNING door not in a wall line at", x, y)
        if B[y][x] == 'D':
            h = B[y][x-1] in '#' and B[y][x+1] in '#'
            v = B[y-1][x] in '#' and B[y+1][x] in '#'
            if not (h or v):
                print("// WARNING crypt door not in a wall line at", x, y)
show(G, "MAP_GROUND")
show(U, "MAP_UPPER")
show(B, "MAP_CRYPT")
