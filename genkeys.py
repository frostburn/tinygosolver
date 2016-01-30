four_keys = []
for i in range(1 << 8):
    player = i % (1 << 4);
    opponent = i >> 4;
    key = 0
    for j in reversed(range(4)):
        key *= 3
        if (1 << j) & player:
            key += 1
        elif (1 << j) & opponent:
            key += 2
    four_keys.append(key)

print "static size_t FOUR_KEYS[256] = " + str(four_keys).replace("[", "{").replace("]", "}") + ";"

five_keys = []
for i in range(1 << 10):
    player = i % (1 << 5);
    opponent = i >> 5;
    key = 0
    for j in reversed(range(5)):
        key *= 3
        if (1 << j) & player:
            key += 1
        elif (1 << j) & opponent:
            key += 2
    five_keys.append(key)

print "static size_t FIVE_KEYS[1024] = " + str(five_keys).replace("[", "{").replace("]", "}") + ";"