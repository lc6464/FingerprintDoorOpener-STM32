#include "Button.h"

bool Button::HandleInterrupt(uint16_t GPIO_Pin) {
	if (GPIO_Pin != _portPin.Pin) {
		return false;
	}

	State newState = (HAL_GPIO_ReadPin(_portPin.Port, _portPin.Pin) == GPIO_PIN_RESET)
		? State::Pressed
		: State::Released;

	UpdateStateAndTriggerCallback(newState);
	return true;
}

void Button::Tick(uint32_t deltaTime/* = 1*/) {
	if (_currentState == State::Pressed) {
		_pressDuration += deltaTime;
		if (_pressDuration >= _longPressDuration) {
			UpdateStateAndTriggerCallback(State::Triggered);
			if (_longPressCallback) {
				_longPressCallback();
			}
		}
	}
}

void Button::UpdateStateAndTriggerCallback(State newState) {
	if (newState != _currentState) {
		if (newState == State::Pressed) {
			_pressDuration = 0;
			if (_pressCallback) {
				_pressCallback();
			}
		} else if (newState == State::Released) {
			if (_currentState == State::Pressed && _shortPressCallback) {
				_shortPressCallback();
			}
			if (_releaseCallback) {
				_releaseCallback();
			}
		}
		_currentState = newState;
	}
}
