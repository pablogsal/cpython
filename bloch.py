import asyncio

async def foo():
    await asyncio.sleep(10)

async def blech(x):
    await blech2(x)

async def blech2(x):
    await blech3(x)

async def blech3(x):
    await x

async def plech(x):
    await plech2(x)

async def plech2(x):
    await plech3(x)

async def plech3(x):
    await x

async def bloch(x, name):
    await blocho_caller(x, name)

async def blocho_caller(x, name):
    async with asyncio.TaskGroup() as tg:
        tg.create_task(blech(x), name=f'child1_{name}')
        tg.create_task(plech(x), name=f'child2_{name}')

async def main():
    t = asyncio.Task(foo(), name='timer')
    async with asyncio.TaskGroup() as tg:
        tg.create_task(bloch(t, "1"), name="root1")
        tg.create_task(bloch(t, "2"), name="root2")

asyncio.run(main())
