from dataclasses import dataclass

@dataclass
class Person:
    name: str

@dataclass
class Movie:
    title: str
    actors: list[Person]
    directors: list[Person]

def matching_test(arg):
    match arg:
        case {"title": title, "actors": [*actors], "directors": [*directors]}:
            return Movie(title=title, actors=[Person(a) for a in actors], directors=[Person(d) for d in directors])

        case {"title": title, "actors": [*actors], "directors": str(director)}:
            return Movie(title=title, actors=[Person(a) for a in actors], directors=[Person(director)])

        case Movie(title="Blade Runner", directors=[Person(name="Riddle Scott")]):
            print("great taste!")
            return arg
        case _:
            return "no match"




