import datetime
import random
import unittest

import numpy as np
import pandas as pd

from context import pdtypes
import pdtypes.cast


random.seed(12345)


class CastFloatToIntegerAccuracyTests(unittest.TestCase):

    ############################
    ####    Whole Floats    ####
    ############################

    def test_whole_float_to_integer_no_na(self):
        # Arrange
        floats = [-2.0, -1.0, 0.0, 1.0, 2.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series)

        # Assert
        expected = pd.Series([-2, -1, 0, 1, 2])
        pd.testing.assert_series_equal(result, expected)

    def test_whole_float_to_integer_with_na(self):
        # Arrange
        floats = [-2.0, -1.0, 0.0, 1.0, 2.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series)

        # Assert
        expected = pd.Series([-2, -1, 0, 1, 2] + [None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_whole_float_to_integer_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    # TODO: Larger than 64-bit?

    ##############################
    ####    Decimal Floats    ####
    ##############################

    def test_decimal_float_to_integer_error(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + random.random() / 2 if idx % 2
                  else i - random.random() / 2
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats)

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series)
        err_msg = (f"[pdtypes.cast.float_to_integer] could not convert series "
                   f"to integer without losing information: "
                   f"{list(input_series.head())}")
        self.assertEqual(str(err.exception), err_msg)

    def test_decimal_float_to_integer_within_tolerance_no_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + 1e-8 if idx % 2 else i - 1e-8
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, tol=1e-6)

        # Assert
        expected = pd.Series(integers)
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_within_tolerance_with_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + 1e-8 if idx % 2 else i - 1e-8
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, tol=1e-6)

        # Assert
        expected = pd.Series(integers + [None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_rounded_no_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + random.random() / 2 if idx % 2
                  else i - random.random() / 2
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, round=True)

        # Assert
        expected = pd.Series(integers)
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_rounded_with_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + random.random() / 2 if idx % 2
                  else i - random.random() / 2
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, round=True)

        # Assert
        expected = pd.Series(integers + [None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_rounded_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, round=True)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_forced_no_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + random.random() / 2 if idx % 2
                  else i - random.random() / 2
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, force=True)

        # Assert
        expected = pd.Series([int(f) for f in floats])
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_forced_with_na(self):
        # Arrange
        integers = [-2, -1, 0, 1, 2]
        floats = [i + random.random() / 2 if idx % 2
                  else i - random.random() / 2
                  for idx, i in enumerate(integers)]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, force=True)

        # Assert
        expected = pd.Series([int(f) for f in floats] + [None],
                             dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_decimal_float_to_integer_forced_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, force=True)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)


class CastFloatToIntegerOutputTypeTests(unittest.TestCase):

    ###########################################
    ####    Correct - Standard Integers    ####
    ###########################################

    def test_float_to_standard_integer_output_type_no_na(self):
        # Arrange
        floats = [-2.0, -1.0, 0.0, 1.0, 2.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=int)

        # Assert
        expected = pd.Series([-2, -1, 0, 1, 2])
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_standard_integer_output_type_with_na(self):
        # Arrange
        floats = [-2.0, -1.0, 0.0, 1.0, 2.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=int)

        # Assert
        expected = pd.Series([-2, -1, 0, 1, 2, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_standard_integer_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=int)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##################################
    ####    Correct - np.uint8    ####
    ##################################

    def test_float_to_numpy_unsigned_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint8)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint8)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint8)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint8))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint8))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint8))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int8_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u1")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int8_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u1")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int8_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u1")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    ###################################
    ####    Correct - np.uint16    ####
    ###################################

    def test_float_to_numpy_unsigned_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint16)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint16)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint16)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint16))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint16))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint16))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int16_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u2")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int16_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u2")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int16_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u2")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    ###################################
    ####    Correct - np.uint32    ####
    ###################################

    def test_float_to_numpy_unsigned_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint32)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint32)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint32)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint32))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint32))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint32))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int32_format_output_type_string_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u4")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int32_format_output_type_string_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u4")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int32_format_output_type_string_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u4")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    ###################################
    ####    Correct - np.uint64    ####
    ###################################

    def test_float_to_numpy_unsigned_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint64)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint64)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.uint64)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint64))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint64))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_unsigned_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.uint64))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int64_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u8")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.uint64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int64_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u8")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_unsigned_int64_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="u8")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    #########################################
    ####    Correct - pd.UInt8Dtype()    ####
    #########################################

    def test_float_to_pandas_unsigned_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt8Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt8Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt8Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt8Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##########################################
    ####    Correct - pd.UInt16Dtype()    ####
    ##########################################

    def test_float_to_pandas_unsigned_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt16Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt16Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt16Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt16Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##########################################
    ####    Correct - pd.UInt32Dtype()    ####
    ##########################################

    def test_float_to_pandas_unsigned_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt32Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt32Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt32Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt32Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##########################################
    ####    Correct - pd.UInt64Dtype()    ####
    ##########################################

    def test_float_to_pandas_unsigned_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt64Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt64Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_unsigned_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.UInt64Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.UInt64Dtype())
        pd.testing.assert_series_equal(result, expected)

    #################################
    ####    Correct - np.int8    ####
    #################################

    def test_float_to_numpy_signed_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int8)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int8)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int8)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int8))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int8))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int8))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int8_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i1")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int8)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int8_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i1")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int8_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i1")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##################################
    ####    Correct - np.int16    ####
    ##################################

    def test_float_to_numpy_signed_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int16)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int16)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int16)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int16))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int16))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int16))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int16_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i2")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int16)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int16_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i2")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int16_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i2")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##################################
    ####    Correct - np.int32    ####
    ##################################

    def test_float_to_numpy_signed_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int32)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int32)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int32)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int32))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int32))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int32))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int32_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i4")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int32)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int32_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i4")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int32_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i4")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##################################
    ####    Correct - np.int64    ####
    ##################################

    def test_float_to_numpy_signed_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int64)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int64)

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype=np.int64)

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int64))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int64))

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_dtype_signed_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=np.dtype(np.int64))

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int64_format_string_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i8")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=np.int64)
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int64_format_string_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i8")

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_numpy_signed_int64_format_string_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series, dtype="i8")

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    ########################################
    ####    Correct - pd.Int8Dtype()    ####
    ########################################

    def test_float_to_pandas_signed_int8_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int8Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int8_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int8Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int8_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int8Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int8Dtype())
        pd.testing.assert_series_equal(result, expected)

    #########################################
    ####    Correct - pd.Int16Dtype()    ####
    #########################################

    def test_float_to_pandas_signed_int16_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int16Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int16_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int16Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int16_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int16Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int16Dtype())
        pd.testing.assert_series_equal(result, expected)

    #########################################
    ####    Correct - pd.Int32Dtype()    ####
    #########################################

    def test_float_to_pandas_signed_int32_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int32Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int32_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int32Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int32_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int32Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int32Dtype())
        pd.testing.assert_series_equal(result, expected)

    #########################################
    ####    Correct - pd.Int64Dtype()    ####
    #########################################

    def test_float_to_pandas_signed_int64_output_type_no_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int64Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int64_output_type_with_na(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats + [None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int64Dtype())

        # Assert
        expected = pd.Series([0, 1, 2, 3, 4, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    def test_float_to_pandas_signed_int64_output_type_only_na(self):
        # Arrange
        input_series = pd.Series([None, None, None])

        # Act
        result = pdtypes.cast.float_to_integer(input_series,
                                               dtype=pd.Int64Dtype())

        # Assert
        expected = pd.Series([None, None, None], dtype=pd.Int64Dtype())
        pd.testing.assert_series_equal(result, expected)

    ##################################
    ####    Incorrect - Floats    ####
    ##################################

    def test_float_to_standard_float_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=float)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {float})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_float16_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.float16)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.float16})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_float32_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.float32)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.float32})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_float64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.float64)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.float64})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_float128_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.float128)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.float128})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_float16_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.float16))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.float16)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_float32_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.float32))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.float32)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_float64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.float64))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.float64)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_float128_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.float128))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.float128)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_float16_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="f2")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: f2)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_float32_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="f4")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: f4)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_float64_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="f8")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: f8)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_float128_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="f16")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: f16)")
        self.assertEqual(str(err.exception), err_msg)

    ###########################################
    ####    Incorrect - Complex Numbers    ####
    ###########################################

    def test_float_to_standard_complex_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=complex)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {complex})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_complex64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.complex64)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.complex64})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_complex128_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.complex128)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.complex128})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_complex64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.complex64))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.complex64)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_complex128_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.complex128))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.complex128)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_complex64_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="c8")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: c8)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_complex128_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="c16")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: c16)")
        self.assertEqual(str(err.exception), err_msg)

    ###################################
    ####    Incorrect - Strings    ####
    ###################################

    def test_float_to_standard_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=str)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {str})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_pandas_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=pd.StringDtype())
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {pd.StringDtype()})")
        self.assertEqual(str(err.exception), err_msg)

    ####################################
    ####    Incorrect - Booleans    ####
    ####################################

    def test_float_to_standard_boolean_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=bool)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {bool})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_pandas_boolean_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=pd.BooleanDtype())
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {pd.BooleanDtype()})")
        self.assertEqual(str(err.exception), err_msg)

    #####################################
    ####    Incorrect - Datetimes    ####
    #####################################

    def test_float_to_standard_datetime_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=datetime.datetime)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {datetime.datetime})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_datetime64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.datetime64)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.datetime64})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_datetime64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.datetime64))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.datetime64)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_datetime64_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="M8")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: M8)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_datetime64_ns_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="M8[ns]")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: M8[ns])")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_pandas_timestamp_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=pd.Timestamp)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {pd.Timestamp})")
        self.assertEqual(str(err.exception), err_msg)

    ######################################
    ####    Incorrect - Timedeltas    ####
    ######################################

    def test_float_to_standard_timedelta_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=datetime.timedelta)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {datetime.timedelta})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_timedelta64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.timedelta64)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.timedelta64})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_timedelta64_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series,
                                          dtype=np.dtype(np.timedelta64))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(np.timedelta64)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_timedelta64_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="m8")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: m8)")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_timedelta64_ns_format_string_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="m8[ns]")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: m8[ns])")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_pandas_timestamp_output_type_error(self):
        # Arrange
        input_series = pd.Series([-2.0, -1.0, 0.0, 1.0, 2.0])

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=pd.Timedelta)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {pd.Timedelta})")
        self.assertEqual(str(err.exception), err_msg)

    ###################################
    ####    Incorrect - Objects    ####
    ###################################

    def test_float_to_standard_object_output_type_error(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=object)
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {object})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_numpy_dtype_object_output_type_error(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype=np.dtype(object))
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: {np.dtype(object)})")
        self.assertEqual(str(err.exception), err_msg)

    def test_float_to_object_format_string_output_type_error(self):
        # Arrange
        floats = [0.0, 1.0, 2.0, 3.0, 4.0]
        input_series = pd.Series(floats)

        # Act - Error
        with self.assertRaises(TypeError) as err:
            pdtypes.cast.float_to_integer(input_series, dtype="O")
        err_msg = (f"[pdtypes.cast.float_to_integer] `dtype` must be "
                   f"int-like (received: O)")
        self.assertEqual(str(err.exception), err_msg)


if __name__ == "__main__":
    unittest.main()
