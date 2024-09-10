#pragma once

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif