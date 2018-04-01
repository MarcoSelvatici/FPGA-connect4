"""
optimizations:
- [DONE] do not create a list of successors, build up a state recursively and then unbuild it returning
- [DONE] use integer values for operations
- do not waste time at the beginning, the algorithm will always return the same values for the first move(s) 
"""

from random import randint
import time


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
        if state is None or heights is None:
            self.state   = [0 for _ in range(42)]
            self.heights = [0 for _ in range(7)]
        else:
            self.state   = state
            self.heights = heights 

    def col_to_idx(self, col):
        return (7 * (6 - self.heights[col])) + col
    
    def ij_to_idx(self, i, j):
        return (i * 7) + j
    
    def insert_coin(self, pos, color):
        if pos < 0 or pos > 6 or self.heights[pos] >= 6:
            raise ValueError("Trying to insert a coin out of limits")
        self.heights[pos] += 1          # the order of this two operations is critical
        self.state[self.col_to_idx(pos)] = color

    def remove_coin(self, pos):
        if pos < 0 or pos > 6 or self.heights[pos] <= 0:
            raise ValueError("Trying to insert a coin out of limits")
        self.state[self.col_to_idx(pos)] = 0      # the order of this two operations is critical
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
        self.player1 = 1
        self.player2 = 2
        self.turn = 0
        self.max_depths = max_depths
        self.default_depth = default_depth  

    def discount(self, val, depth):
        if val > 0:
            return self.DISCOUNT * depth + val
        elif val < 0:
            return self.DISCOUNT * depth + val
        else:
            return val

    def can_insert_coin(self, state, pos):
        return (state.heights[pos] <= 5)

    def max_node(self, state, depth, alpha, beta):
        if state.is_win(): # human won
            return self.discount(-self.WIN, depth)
        if(depth == self.max_depth):
            return self.discount(self.CONNECT3 * state.evaluate(), depth)

        max_val = -self.INF
        for idx in range(7):
            if not self.can_insert_coin(state, idx):
                continue
            state.insert_coin(idx, self.player1) # insert coin and go down with recursion
            max_val = max(max_val, self.min_node(state, depth + 1, alpha, beta))
            state.remove_coin(idx) # remove coin to get the previous state
            if max_val >= beta:
                return max_val
            alpha = max(alpha, max_val)
        return self.discount(max_val, depth)

    def min_node(self, state, depth, alpha, beta):
        if state.is_win(): # pc won
            return self.discount(self.WIN, depth)
        if(depth == self.max_depth):
            return self.discount(self.CONNECT3 * state.evaluate(), depth)

        min_val = self.INF
        for idx in range(7):
            if not self.can_insert_coin(state, idx):
                continue
            state.insert_coin(idx, self.player2) # insert coin and go on with recursion   
            min_val = min(min_val, self.max_node(state, depth + 1, alpha, beta))
            state.remove_coin(idx) # remove coin to get the previous state
            if min_val <= alpha:
                return min_val
            beta = max(beta, min_val)
        return min_val

    def get_move(self, state):
        print("getting values for moves: ", end="")
        self.turn += 1

        # get max depth
        self.max_depth = self.default_depth
        if self.turn in self.max_depths.keys():
            self.max_depth = self.max_depths[self.turn]
        print("[with depth ", str(self.max_depth) + "]")

        start = time.time()

        max_val = -self.INF
        alpha   = -self.INF
        best_moves = []
        for idx in range(7):
            if not self.can_insert_coin(state, idx):
                continue
            state.insert_coin(idx, self.player1)
            tmp = self.min_node(state, 1, alpha, self.INF)
            state.remove_coin(idx)
            print(tmp, end=" ")
            if tmp > max_val:
                best_moves.clear()
                max_val = tmp
                best_moves.append(idx)
                alpha = max_val
            elif tmp == max_val:
                best_moves.append(idx)

        end = time.time()
        print("\nTime elapsed:", end - start)
        
        return best_moves[randint(0, len(best_moves)-1)]


def main():
    # turn | depth
    #  1   |   1
    #  2   |   4
    #  3   |   5
    # ...
    max_depths = {1: 1, 2: 4, 3: 5, 4: 6, 5: 6, 6: 7, 7: 7, 8: 8, 9: 8, 10: 9, 11: 9, 12: 10, 13: 10}
    default_depth = 15

    curr_state = Game_state()
    agent = Minimax_agent(max_depths, default_depth)

    curr_state.print()

    turn = 2 # human
    while not curr_state.is_win():
        if turn == 2:
            pos = int(input("your move: "))
            curr_state.insert_coin(pos, 2)
            turn = 1
        else:
            pos = agent.get_move(curr_state)
            curr_state.insert_coin(pos, 1)
            turn = 2
        print("\n_________________\n")
        curr_state.print()
    print("Game over")
    
if __name__ == "__main__":
    main()    
