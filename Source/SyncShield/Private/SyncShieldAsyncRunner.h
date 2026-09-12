// Copyright (c) 2026 GregOrigin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Widgets/SWidget.h"

namespace SyncShieldAsync
{
	/**
	 * Runs Work on the thread pool, then OnGameThread on the game thread, but only if
	 * Owner is still alive. Never pins Owner on the pool: releasing the final reference
	 * to an SWidget off the game thread is a crash.
	 */
	template <typename TResult>
	void RunForOwner(TWeakPtr<SWidget> Owner,
					 TFunction<TResult()> Work,
					 TFunction<void(const TResult&)> OnGameThread)
	{
		Async(EAsyncExecution::ThreadPool,
			// mutable: without it the captured copies are const and the MoveTemp
			// into the continuation fails to compile.
			[Owner, Work = MoveTemp(Work), OnGameThread = MoveTemp(OnGameThread)]() mutable
			{
				// Deliberately does not pin Owner here. Holding the last reference to
				// an SWidget on the pool would run ~SWidget off the game thread.
				TResult Result = Work();

				AsyncTask(ENamedThreads::GameThread,
					[Owner, Result = MoveTemp(Result), OnGameThread = MoveTemp(OnGameThread)]()
					{
						if (!Owner.IsValid())
						{
							return;
						}
						OnGameThread(Result);
					});
			});
	}
}
