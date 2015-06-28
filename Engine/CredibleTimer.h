#pragma once
#include <functional>

using namespace std;

namespace Engine
{
	class CredibleTimer
	{
	public:

		CredibleTimer();

		//
		// Resets the timer and starts counting
		//
		inline void Start(
			_In_ float expireTime,
			_In_ ULONG64 expiresAfter,
			_In_ function<void(void)> f)
		{
			_timer = 0;
			_callbackCount = 0;
			_callbackExpireTime = expireTime;
			_callback = f;
			_active = true;
			_expireCount = expiresAfter;

			_infinite = (_expireCount == 0);
		}

		//
		// Stops the timer but doesn't reset the data
		//
		inline void Stop()
		{
			_active = false;
		}

		//
		// Returns bool indicating if still counting
		//
		inline bool IsActive() const
		{
			return _active;
		}

		//
		// Gets the number of times the callback has been called since starting
		//
		inline ULONG64 GetCallBackCount() const
		{
			return _callbackCount;
		}

		//
		// Updates the counter
		//
		void Update(_In_ float timeDelta);

	private:

		float _timer;

		// How long to wait before calling the callback
		float _callbackExpireTime;

		// The number of times to callback before stopping (0 for infinite)
		ULONG64 _expireCount;

		// The number of times the timer expired and the callback function was called
		ULONG64 _callbackCount;

		bool _active;

		// Indicates if there is no expire callback count
		bool _infinite;

		// The function to callback every _callbackExpireTime
		function<void(void)> _callback;
	};
}
