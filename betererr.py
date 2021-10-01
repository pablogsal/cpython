### DATA ###

name_to_id = {
        "ambv": 0,
        "mariatta": 1,
        "pablogsal": 2,
        "vstinner": 3,
        "yselivanov": 4


### PUBLIC FUNCTIONS ###

def main():
    print(name_to_id(sys.argv[1]))

if __name__ == "__main__":
    main()
