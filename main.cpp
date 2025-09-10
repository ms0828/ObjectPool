#include <iostream>
#include <stack>
#include <vector>
#include "ObjectPool.h"

using namespace std;


struct Point {
    int x, y;
    Point(int _x, int _y) : x(_x), y(_y) {}
};


// x든 y든 더 많이 가는 방향을 기준으로 반복
vector<Point> Bresenham(int x0, int y0, int x1, int y1) {
    vector<Point> result;

    int dx = x1 - x0;
    int dy = y1 - y0;

    int steps = max(abs(dx), abs(dy));  // 총 몇 칸 갈지 (많이 가는 쪽 기준)

    // x, y는 한 칸 갈 때마다 얼마나 움직여야 하는지 (실수로 계산)
    float xStep = dx / (float)steps;
    float yStep = dy / (float)steps;

    // 시작 위치는 float로
    float x = x0;
    float y = y0;

    for (int i = 0; i <= steps; ++i)
    {
        result.push_back(Point((int)x, (int)y)); // 현재 위치 저장 (반올림)
        x += xStep;
        y += yStep;
    }

    return result;
}

int main()
{
    /*int x0 = 0, y0 = 0;
    int x1 = 8, y1 = 4;

    vector<Point> path = Bresenham(x0, y0, x1, y1);

    cout << "지나는 칸들:" << endl;
    for (auto& p : path) {
        cout << "(" << p.x << ", " << p.y << ")" << endl;
    }*/

    int q = 0;

    int a = 10035;

    int t = 0;

    return 0;
}