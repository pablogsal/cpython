import asyncio
import time
import os

async def test_stack_tgroup():

        stack_for_c5 = None

        def c5():
            nonlocal stack_for_c5
            time.sleep(1000)

        async def c4():
            await asyncio.sleep(0)
            c5()

        async def c3():
            await c4()

        async def c2():
            await c3()

        async def c1(task):
            await task

        async def main():
            async with asyncio.TaskGroup() as tg:
                task = tg.create_task(c2(), name="c2_root")
                tg.create_task(c1(task), name="sub_main_1")
                tg.create_task(c1(task), name="sub_main_2")

        await main()

print(os.getpid())
asyncio.run(test_stack_tgroup())
