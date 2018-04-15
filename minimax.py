"""
optimizations:
- [DONE] do not create a list of successors, build up a state recursively and then unbuild it returning
- [DONE] use integer values for operations
- do not waste time at the beginning, the algorithm will always return the same values for the first move(s) 
"""

from random import randint
import time # measure performance
import sys  # flush print

"""
Game_state

0 -> blank
1 -> red  (computer)
2 -> blue (human)
3 -> error

state:
  __j__
| 0 0 1
i 1 0 2
| 2 1 2

is represented as:
0 0 1  1 0 2  2 1 2


(i,j)
(0,0) (0,1) (0,2) (0,3)
(1,0) (1,1) (1,2) (1,3)
(2,0) (2,1) (2,2) (2,3)

"""
class Game_state:
    def __init__(self, state=None, heights=None):
        self.state   = [0 for _ in range(42)]
        self.heights = [0 for _ in range(7)]

    def col_to_idx(self, col):
        return (7 * (6 - self.heights[col])) + col
    
    def ij_to_idx(self, i, j):
        return (i * 7) + j

    def idx_to_ij(self, idx):
        return (int(idx / 7), idx % 7)
    
    def insert_coin(self, pos, color):
        self.heights[pos] += 1 # the order of this two operations is critical
        self.state[self.col_to_idx(pos)] = color

    def remove_coin(self, pos):
        self.state[self.col_to_idx(pos)] = 0 # the order of this two operations is critical
        self.heights[pos] -= 1

    def check_connect4(self, f1, f2, i, j):
        tmp = 1
        for k in range(1, 4): # [1,2,3]
            if self.state[self.ij_to_idx( i + k*f1, j + k*f2 )] == self.state[self.ij_to_idx(i,j)]:
                tmp += 1
            else:
                break
        return (tmp == 4)

    def is_win(self):
        for i in range(6):
            for j in range(7):
                if self.state[self.ij_to_idx(i,j)] != 0:
                    # horizontal
                    if (j + 3 < 7) and self.check_connect4(0, 1, i, j):
                        return True
                    # vertical
                    if (i + 3 < 6) and self.check_connect4(1, 0, i, j):
                        return True
                    # oblique up
                    if (i + 3 < 6 and j + 3 < 7) and self.check_connect4(1, 1, i, j):
                        return True
                    # oblique down
                    if (i - 3 >= 0 and j + 3 < 7) and self.check_connect4(-1, 1, i, j):
                        return True
        return False

    def count_connected(self, f1, f2, i, j):
        tmp = 0
        for k in range(1, 4): # [1,2,3]
            if i + k*f1 >= 0 and i + k*f1 < 6 and j + k*f2 >= 0 and j + k*f2 < 7 and \
               self.state[self.ij_to_idx( i + k*f1, j + k*f2 )] == self.state[self.ij_to_idx(i,j)]:
                tmp += 1
            else:
                break
        return tmp
    
    def is_win_fast(self, col):
        i, j = self.idx_to_ij(self.col_to_idx(col))
        # horizontal
        if(self.count_connected(0, 1, i, j) + self.count_connected(0, -1, i, j) + 1 >= 4):
            return True
        # vertical
        if(self.count_connected(1, 0, i, j) + self.count_connected(-1, 0, i, j) + 1 >= 4):
            return True
        # oblique 
        if(self.count_connected(1, 1, i, j) + self.count_connected(-1, -1, i, j) + 1 >= 4):
            return True
        if(self.count_connected(-1, 1, i, j) + self.count_connected(1, -1, i, j) + 1 >= 4):
            return True
        return False

    def count_connect3(self, f1, f2, i, j):
        tmp = 1
        for k in range(1, 3): # [1, 2]
            if self.state[self.ij_to_idx( i + k*f1, j + k*f2 )] == self.state[self.ij_to_idx(i,j)]:
                tmp += 1
            elif (self.state[self.ij_to_idx( i + k*f1, j + k*f2 )] != self.state[self.ij_to_idx(i,j)]) and (self.state[self.ij_to_idx( i + k*f1, j + k*f2 )] != 0):
                tmp = 0
                break
        
        if tmp == 3:
            if self.state[self.ij_to_idx(i,j)] == 1: # player 1
                return 1
            else:                                    # player 2
                return -1
        return 0
    
    def evaluate(self):
        tot_count = 0
        for i in range(6):
            for j in range(7):
                if self.state[self.ij_to_idx(i,j)] != 0:
                    # horizontal
                    if j + 2 < 7:
                        tot_count += self.count_connect3(0, 1, i, j)
                    # vertical
                    if i + 2 < 6:
                        tot_count += self.count_connect3(1, 0, i, j)
                    # oblique up
                    if i + 2 < 6 and j + 2 < 7:
                        tot_count += self.count_connect3(1, 1, i, j)
                    # oblique down
                    if i - 2 >= 0 and j + 2 < 7:
                        tot_count += self.count_connect3(-1, 1, i, j)
        return tot_count
    
    def print(self):
        idx = 0
        char = ['-', 'X', 'O']
        for _ in range(6):
            for __ in range(7):
                print(char[self.state[idx]], end=' ')
                idx += 1
            print()
        print("_____________")
        for i in range(7):
            print(i, end=" ")
        print("\n")


class Minimax_agent:
    def __init__(self, max_depths, default_depth):
        self.INF = 99999999
        self.WIN = 10000
        self.CONNECT3 = 50
        self.DISCOUNT = 1
        self.PLAYER1 = 1
        self.PLAYER2 = 2
        self.columns = [3, 4, 2, 5, 1, 6, 0]
        self.turn = 0
        self.max_depths = max_depths
        self.default_depth = default_depth 
        self.turn_times = []

    def print_turn_times(self):
        for i, time in enumerate(self.turn_times):
            print("%d. time: %.4f\t\t | depth: %d" % (i+1, time, self.get_max_depth(i+1)))

    def discount(self, val, depth):
        if val > 0:
            return -self.DISCOUNT * depth + val
        elif val < 0:
            return self.DISCOUNT * depth + val
        else:
            return val

    def can_insert_coin(self, state, pos):
        return (state.heights[pos] <= 5)

    def get_max_depth(self, turn, print_val=False):
        max_depth = self.default_depth
        if turn in self.max_depths.keys():
            max_depth = self.max_depths[turn]
        if print_val:
            print("[with depth ", str(max_depth) + "]")
        return max_depth

    def max_node(self, state, depth, alpha, beta):
        if(depth == self.max_depth):
            return self.discount(self.CONNECT3 * state.evaluate(), depth)

        max_val = -self.INF
        for col in self.columns:
            if not self.can_insert_coin(state, col):
                continue

            state.insert_coin(col, self.PLAYER1) # insert coin and go down with recursion
            # chek if this move lead to a win
            if state.is_win_fast(col):
                #max_val = max(max_val, self.discount(self.WIN, depth + 1))
                state.remove_coin(col)
                return self.discount(self.WIN, depth + 1)
            else: # if not, we have to calculate everything normally
                max_val = max(max_val, self.min_node(state, depth + 1, alpha, beta))
            state.remove_coin(col) # remove coin to get the previous state

            if max_val > beta:
                return max_val
            alpha = max(alpha, max_val)

        if(max_val == -self.INF): # the game already ended with a tie and no moves have been done here
            return 0
        return self.discount(max_val, depth)

    def min_node(self, state, depth, alpha, beta):
        if(depth == self.max_depth):
            return self.discount(self.CONNECT3 * state.evaluate(), depth)

        min_val = self.INF
        for col in self.columns:
            if not self.can_insert_coin(state, col):
                continue

            state.insert_coin(col, self.PLAYER2) # insert coin and go on with recursion   
            # chek if this move lead to a win
            if state.is_win_fast(col):
                #min_val = min(min_val, self.discount(-self.WIN, depth))
                state.remove_coin(col)
                return self.discount(-self.WIN, depth)
            else: # if not, we have to calculate everything normally
                min_val = min(min_val, self.max_node(state, depth + 1, alpha, beta))
            state.remove_coin(col) # remove coin to get the previous state

            if min_val < alpha:
                return min_val
            beta = min(beta, min_val)

        if(min_val == self.INF): # the game already ended with a tie and no moves have been done here
            return 0
        return self.discount(min_val, depth)

    def get_move(self, state):
        print("getting values for moves: ", end="")
        self.turn += 1

        # get max depth
        self.max_depth = self.get_max_depth(self.turn, True)

        start = time.time()

        max_val = -self.INF
        alpha = -self.INF
        best_moves = []
        for col in self.columns:
            if not self.can_insert_coin(state, col):
                continue
            tmp = 0
            state.insert_coin(col, self.PLAYER1)
            if state.is_win_fast(col):
                tmp = self.WIN
            else:
                tmp = self.min_node(state, 1, alpha, self.INF)
            state.remove_coin(col)
            
            print(tmp, end=" ")
            sys.stdout.flush() # avoid buffering on print
            
            if tmp > max_val:
                best_moves.clear()
                max_val = tmp
                alpha = tmp
                best_moves.append(col)
            elif tmp == max_val:
                best_moves.append(col)

        end = time.time()
        print("\nTime elapsed:", end - start)
        self.turn_times.append(end - start)
        
        return best_moves[randint(0, len(best_moves)-1)]

def main():
    # turn | depth
    #  1   |   1
    #  2   |   4
    #  3   |   5
    # ...

    
    curr_state = Game_state()
    """ oblique
    for _ in range(3):
        curr_state.insert_coin(3, 1)
    for _ in range(2):
        curr_state.insert_coin(4, 1)
    curr_state.insert_coin(5, 1)

    for i in range(4):
        curr_state.insert_coin(3 + i, 2)
    
    # other oblique
    for _ in range(3):
        curr_state.insert_coin(6, 1)
    for _ in range(2):
        curr_state.insert_coin(5, 1)
    curr_state.insert_coin(4, 1)

    for i in range(4):
        curr_state.insert_coin(3 + i, 2)
    
    curr_state.print()
    print(curr_state.is_win_fast(3))

    """
    max_depths = {1: 1, 2: 4, 3: 5, 4: 6, 5: 6, 6: 6, 7: 6, 8: 7, 9: 7, 10: 7, 11: 8, 12: 8, 13: 9, 14: 9, 15: 10}
    default_depth = 15

    curr_state = Game_state()
    agent = Minimax_agent(max_depths, default_depth)

    curr_state.print()

    turn = 0 # human
    while not curr_state.is_win() and turn < 42:
        if turn % 2 == 0:
            pos = int(input("Your move: "))
            while pos < 0 or pos > 6 or curr_state.heights[pos] >= 6:
                print("Trying to insert a coin in an invalid position")
                pos = int(input("Your move: "))
            curr_state.insert_coin(pos, 2)
        else:
            pos = agent.get_move(curr_state)
            curr_state.insert_coin(pos, 1)
        turn += 1
        print("\n_____________________\n")
        curr_state.print()
    print("Game over")
    agent.print_turn_times()
    
    
if __name__ == "__main__":
    main()    
