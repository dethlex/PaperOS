#pragma once
#include <stdint.h>

namespace paperos {

// Оценка заряда (%) одной Li-Po банки по напряжению покоя (мВ).
// Кусочно-линейная интерполяция по зашитой таблице, результат клампится 0..100.
uint8_t batteryPercentFromMv(uint16_t mv);

} // namespace paperos
