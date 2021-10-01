def matching_test(arg):
    match arg:
        case "hey":
            return "Hi"
        case str():
            return "some str"
        case ["hey"]:
            return "[hi]"
        case ["hey", "there"]:
            return "[hi, yourself]"
        case ["hey", "jude" | "joe"]:
            return "[hi, person of culture]"
        case ["hey", *words]:
            return "[hi, " + ", ".join(words) + "]"
        case _:
            return "something else"
