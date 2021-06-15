#
# Author: sascha_lammers@gmx.de
#

def dump_matrix(width, height, direction=(1, None), fmt='\'%03u\': \' \', '):

    matrix={}
    n = 0

    for i in range(0, width):
        matrix[i]={}

    for i in range(0, width):
        odd_cols = (i%2==direction[0])
        for j in range(0, height):
            odd_rows = (j%2==direction[1])
            matrix[(odd_rows and (width - i) or (i + 1)) - 1][(odd_cols and (height - j) or (j + 1)) - 1] = (direction[1] and (j * width + i + 1) or (n + 1)) - 1
            n += 1

    for j in range(0, height):
        s = '{ '
        for i in range(0, width):
            s += fmt % matrix[i][j]
        s = s.rstrip(', ') + ' },'
        print(s)


# print('-'*76)
# dump_matrix(32, 8, (None, 1))
# print('-'*76)
dump_matrix(32, 8, (1,  None))
print('-'*76)
# sys.exit(0)
