import math

T = 28.78
RH = 34.85


def LN(n):
    return math.log(n)

def EXP(n):
    return math.exp(n)

TD=243.04*(LN(RH/100)+((17.625*T)/(243.04+T)))/(17.625-LN(RH/100)-((17.625*T)/(243.04+T)))
print(TD)

T=25.03

RH2=100*(EXP((17.625*TD)/(243.04+TD))/EXP((17.625*T)/(243.04+T)))

print(RH2)

# T=243.04*(((17.625*TD)/(243.04+TD))-LN(RH/100))/(17.625+LN(RH/100)-((17.625*TD)/(243.04+TD)))
