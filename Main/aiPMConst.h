#ifndef __aiPMConst_H_
#define __aiPMConst_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NAI
{
const int N_RECALC_GRID_SIZE = 16;
// радиус сферы для проверки возможности поставить юнит в ту или иную точку
const float F_TEST_SPHERE_RADIUS = 0.31f;
// высота одного этажа
const float F_WALL_HEIGHT = 2.5f;
// радиус сферы для проверки возможности спрыгнуть или залезть 
const float F_HC_TEST_SPHERE_RADIUS = 0.28f;
// высота юнита
const float F_HEIGHT = 1.9f;
// высота юнита в положении в раскоряку
const float F_SPECIAL_HEIGHT = 1;
// высота, на которую нужно ставить сферу для проверки на 30 градусное ограничение
const float F_CHECK_HEIGHT = F_TEST_SPHERE_RADIUS * 2.0f / SQRT_3 + 0.05f;
// высота, на которую нужно приподнимать сферу для проверки возможности прохода
const float F_CHECK_HEIGHT_MOVE = F_TEST_SPHERE_RADIUS + 0.05f;
// шаг, на который поднимается сфера при проверке на то, что в точке можно сидеть (+еще столько же и можно стоять)
const float F_TEST_SPHERE_STEP = ( F_HEIGHT - F_CHECK_HEIGHT - F_TEST_SPHERE_RADIUS ) * 0.5f;
// шаг, на который поднимается сфера для проверки проходимости в другой позе
const float F_TEST_SPHERE_STEP_MOVE = ( F_HEIGHT - F_CHECK_HEIGHT_MOVE - F_TEST_SPHERE_RADIUS ) * 0.5f;
// высота, на которую поднимается вторая сфера для проверки на inactive
const float F_SPECIAL_STEP = ( F_SPECIAL_HEIGHT - F_CHECK_HEIGHT - F_TEST_SPHERE_RADIUS );
// максимальная высота особой точки
const float F_MAX_SPECIAL_POINT_HEIGHT = 2.8f;
// шаг, с которым ищется особая точка
const float F_SPECIAL_POINT_TEST_STEP = 0.1f;
// максимальная разница высот при которой все еще возможен нормальный переход
const float F_NORMAL_HEIGHT_DIFF_LIMIT = 0.55f; //FP_GRID_STEP / SQRT_3;
// то же, что и F_NORMAL_HEIGHT_DIFF_LIMIT, но для диагональных переходов
const float F_DIAGONAL_HEIGHT_DIFF_LIMIT = F_NORMAL_HEIGHT_DIFF_LIMIT * SQRT_2;
const float F_SPECIAL_SPHERE_MULTIPLIER = 0.8f;
// высота, минимально разделяющая точки на одном этаже, расположенные друг над другом, так, 
// чтобы их можно было счесть находящимися на разных лэйерах
const float F_MIN_LAYER_WIDTH = F_TEST_SPHERE_RADIUS + F_CHECK_HEIGHT;
// радиус сферки, проверяющей, жива ли лестница
const float F_LADDER_TEST_RADIUS = 0.1f;
// расстояние до точки, в которой проверяется наличие лестницы по горизонтали (т.е. "размах рук" при ползании по лестнице)
const float F_LADDER_SHIFT = 0.5f;
}
#endif
