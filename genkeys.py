for N, name in [(4, "FOUR"), (5, "FIVE"), (6, "SIX")]:
    keys = []
    for i in range(1 << (2 * N)):
        player = i % (1 << N);
        opponent = i >> N;
        key = 0
        for j in reversed(range(N)):
            key *= 3
            if (1 << j) & player:
                key += 1
            elif (1 << j) & opponent:
                key += 2
        keys.append(key)

    print "static size_t " + name + "_KEYS[" + str(1 << (2 * N)) + "] = " + str(keys).replace("[", "{").replace("]", "}") + ";"
