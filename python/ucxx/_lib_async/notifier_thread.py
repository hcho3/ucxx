# Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
# Copyright (c) 2020       UT-Battelle, LLC. All rights reserved.
# See file LICENSE for terms.

import asyncio
import logging

import ucxx._lib.libucxx as ucx_api

logger = logging.getLogger("ucx")


async def _run_request_notifier(worker):
    worker.run_request_notifier()


async def _notifier_coroutine(worker):
    worker.populate_python_futures_pool()
    finished = worker.wait_request_notifier()
    if finished:
        return True

    # Notify all enqueued waiting futures
    await _run_request_notifier(worker)

    return False


def _notifierThread(event_loop, worker, q):
    logger.debug("Starting Notifier Thread")
    asyncio.set_event_loop(event_loop)
    shutdown = False

    while True:
        worker.populate_python_futures_pool()
        state = worker.wait_request_notifier(period_ns=int(1e9))  # 1 second timeout

        if not q.empty():
            q_val = q.get()
            if q_val == "shutdown":
                logger.warning("_notifierThread shutting down")
                shutdown = True
            else:
                logger.warning(
                    f"_notifierThread got unknown message from IPC queue: {q_val}"
                )

        if state == ucx_api.PythonRequestNotifierWaitState.Shutdown or shutdown is True:
            return
        elif state == ucx_api.PythonRequestNotifierWaitState.Timeout:
            continue

        # Notify all enqueued waiting futures
        task = asyncio.run_coroutine_threadsafe(
            _run_request_notifier(worker), event_loop
        )
        task.result()
