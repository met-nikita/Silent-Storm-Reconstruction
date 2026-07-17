#ifndef __aiPMConst_H_
#define __aiPMConst_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NAI
{
// retail = 8 (RefreshSpot @0x441c70 >>3, RecalcSquare @0x441010 *8, BuildLayersGroup @0x443290
// (n+7)>>3). ON THE SAVE WIRE: squareLevel dims + CLayersSetTracker::p are serialized in these
// units -- 16 here made every retail-format save's tracker fire with 2x square coords
// (degenerate recalc regions). Dev saves made with 16 are incompatible with this value.
const int N_RECALC_GRID_SIZE = 8;
// sphere radius for testing tile passability (also used by other checks)
const float F_TEST_SPHERE_RADIUS = 0.31f;
// ������ ������ �����
const float F_WALL_HEIGHT = 2.5f;
// ������ ����� ��� �������� ����������� ��������� ��� ������� 
const float F_HC_TEST_SPHERE_RADIUS = 0.28f;
// ������ �����
const float F_HEIGHT = 1.9f;
// ������ ����� � ��������� � ���������
const float F_SPECIAL_HEIGHT = 1;
// ������, �� ������� ����� ������� ����� ��� �������� �� 30 ��������� �����������
const float F_CHECK_HEIGHT = F_TEST_SPHERE_RADIUS * 2.0f / SQRT_3 + 0.05f;
// ������, �� ������� ����� ������������ ����� ��� �������� ����������� �������
const float F_CHECK_HEIGHT_MOVE = F_TEST_SPHERE_RADIUS + 0.05f;
// ���, �� ������� ����������� ����� ��� �������� �� ��, ��� � ����� ����� ������ (+��� ������� �� � ����� ������)
const float F_TEST_SPHERE_STEP = ( F_HEIGHT - F_CHECK_HEIGHT - F_TEST_SPHERE_RADIUS ) * 0.5f;
// ���, �� ������� ����������� ����� ��� �������� ������������ � ������ ����
const float F_TEST_SPHERE_STEP_MOVE = ( F_HEIGHT - F_CHECK_HEIGHT_MOVE - F_TEST_SPHERE_RADIUS ) * 0.5f;
// ������, �� ������� ����������� ������ ����� ��� �������� �� inactive
const float F_SPECIAL_STEP = ( F_SPECIAL_HEIGHT - F_CHECK_HEIGHT - F_TEST_SPHERE_RADIUS );
// ������������ ������ ������ �����
const float F_MAX_SPECIAL_POINT_HEIGHT = 2.8f;
// ���, � ������� ������ ������ �����
const float F_SPECIAL_POINT_TEST_STEP = 0.1f;
// ������������ ������� ����� ��� ������� ��� ��� �������� ���������� �������
const float F_NORMAL_HEIGHT_DIFF_LIMIT = 0.55f; //FP_GRID_STEP / SQRT_3;
// �� ��, ��� � F_NORMAL_HEIGHT_DIFF_LIMIT, �� ��� ������������ ���������
const float F_DIAGONAL_HEIGHT_DIFF_LIMIT = F_NORMAL_HEIGHT_DIFF_LIMIT * SQRT_2;
const float F_SPECIAL_SPHERE_MULTIPLIER = 0.8f;
// ������, ���������� ����������� ����� �� ����� �����, ������������� ���� ��� ������, ���, 
// ����� �� ����� ���� ������ ������������ �� ������ �������
const float F_MIN_LAYER_WIDTH = F_TEST_SPHERE_RADIUS + F_CHECK_HEIGHT;
// ������ ������, �����������, ���� �� ��������
const float F_LADDER_TEST_RADIUS = 0.1f;
// ���������� �� �����, � ������� ����������� ������� �������� �� ����������� (�.�. "������ ���" ��� �������� �� ��������)
const float F_LADDER_SHIFT = 0.5f;
}
#endif
