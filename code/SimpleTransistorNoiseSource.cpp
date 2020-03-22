#include "SimpleTransistorNoiseSource.h"
#include "RNG.h"
#include "Crypto.h"
#include "random.h"
#include <Arduino.h>

SimpleTransistorNoiseSource::SimpleTransistorNoiseSource()
{
}

SimpleTransistorNoiseSource::~SimpleTransistorNoiseSource()
{
}

bool SimpleTransistorNoiseSource::calibrating() const
{
}

void SimpleTransistorNoiseSource::stir()
{
	uint8_t randomdata[32];
	randomness_get_raw_random_bits(randomdata, sizeof(randomdata));
    output(randomdata, sizeof(randomdata), sizeof(randomdata) * 2);
}
