/*
 *  Class TestAudioUtils
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <array>

#include <QTest>

#include "libmyth/audio/pink.h"

class TestPinkNoise: public QObject
{
    Q_OBJECT

  private slots:
    static void PinkNoiseGenerator(void)
    {
        constexpr int kPinkTestSize = 1024;
        std::array<float, kPinkTestSize> output {};

        // Most platforms
        float expected_start[16] = {
            -0.0544319004,  0.0397785045,  0.0384280831, -0.0067043304,
            -0.0748048648, -0.0123084011, -0.0259181894, -0.0571565628,
             0.0145509029, -0.0049047540, -0.0328592546, -0.0279113688,
            -0.1104150862, -0.0459254496,  0.0312881768,  0.0633983016
        };
        float expected_end[16] = {
             0.0850152373,  0.1559465975,  0.1554224938,  0.0848656893,
             0.1315491647,  0.0597126484,  0.0002707243,  0.0033376988,
            -0.0000428873, -0.0444181673, -0.0516466387, -0.0104961181,
             0.0609781742,  0.0504443944,  0.0259989072,  0.0029377376
        };

        pink_noise_t pink;
        initialize_pink_noise(&pink, 16);
        for (auto & val : output)
            val = generate_pink_noise_sample(&pink);

        QCOMPARE(output[   0], expected_start[ 0]);
        QCOMPARE(output[   1], expected_start[ 1]);
        QCOMPARE(output[   2], expected_start[ 2]);
        QCOMPARE(output[   3], expected_start[ 3]);
        QCOMPARE(output[   4], expected_start[ 4]);
        QCOMPARE(output[   5], expected_start[ 5]);
        QCOMPARE(output[   6], expected_start[ 6]);
        QCOMPARE(output[   7], expected_start[ 7]);
        QCOMPARE(output[   8], expected_start[ 8]);
        QCOMPARE(output[   9], expected_start[ 9]);
        QCOMPARE(output[  10], expected_start[10]);
        QCOMPARE(output[  11], expected_start[11]);
        QCOMPARE(output[  12], expected_start[12]);
        QCOMPARE(output[  13], expected_start[13]);
        QCOMPARE(output[  14], expected_start[14]);
        QCOMPARE(output[  15], expected_start[15]);

        QCOMPARE(output[1008], expected_end[ 0]);
        QCOMPARE(output[1009], expected_end[ 1]);
        QCOMPARE(output[1010], expected_end[ 2]);
        QCOMPARE(output[1011], expected_end[ 3]);
        QCOMPARE(output[1012], expected_end[ 4]);
        QCOMPARE(output[1013], expected_end[ 5]);
        QCOMPARE(output[1014], expected_end[ 6]);
        QCOMPARE(output[1015], expected_end[ 7]);
        QCOMPARE(output[1016], expected_end[ 8]);
        QCOMPARE(output[1017], expected_end[ 9]);
        QCOMPARE(output[1018], expected_end[10]);
        QCOMPARE(output[1019], expected_end[11]);
        QCOMPARE(output[1020], expected_end[12]);
        QCOMPARE(output[1021], expected_end[13]);
        QCOMPARE(output[1022], expected_end[14]);
        QCOMPARE(output[1023], expected_end[15]);
    }
};
