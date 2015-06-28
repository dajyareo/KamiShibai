#include "pch.h"
#include "CredibleTimer.h"

using namespace Engine;
using namespace std;

CredibleTimer::CredibleTimer() 
	: _active(false), _timer(0), _callbackExpireTime(0), _expireCount(0), _callbackCount(0), _infinite(false), _callback(nullptr)
{}

void CredibleTimer::Update(_In_ float timeDelta)
{
	if (!_active)
	{
		return;
	}

	_timer += timeDelta;

	if (_timer >= _callbackExpireTime)
	{
		_timer = 0;
		++_callbackCount;

		if (!_infinite)
		{
			if (_callbackCount == _expireCount)
			{
				_active = false;
			}
		}

		_callback();
	}
}