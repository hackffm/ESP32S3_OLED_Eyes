/***************************************************
Copyright (c) 2020 Luis Llamas
(www.luisllamas.es)

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at your option) any later version. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License along with this program.  If not, see <http://www.gnu.org/licenses 
****************************************************/

#ifndef _FACEBEHAVIOR_h
#define _FACEBEHAVIOR_h

#include <Arduino.h>
#include "FaceEmotions.hpp"
#include "AsyncTimer.h"

class Face;

class FaceBehavior
{
 protected:
	Face&  _face;

 public:
	FaceBehavior(Face& face);

	eEmotions CurrentEmotion;

	float Emotions[eEmotions::EMOTIONS_COUNT];

	AsyncTimer Timer;

	void SetEmotion(eEmotions emotion, float value);
	float GetEmotion(eEmotions emotion);
  const char* GetEmotionName(eEmotions emotion);

	void Clear();
	void Update();
	eEmotions GetRandomEmotion();

	void GoToEmotion(eEmotions emotion);
};

#endif

