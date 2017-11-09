# -*- coding: utf-8; -*-
import unittest
import earl

class TestEarlPacking(unittest.TestCase):
    def test_smallint(self):
        self.assertEqual(earl.pack(10), bytes([131,97,10]))

    def test_bigint(self):
        self.assertEqual(earl.pack(1200), bytes([131,98,0,0,4,176]))

    def test_floats(self):
        self.assertEqual(earl.pack(3.141592), bytes([131,70,64,9,33,250,252,139,0,122]))

    def test_map(self):
        self.assertEqual(earl.pack({"d":10}), bytes([131,116,0,0,0,1,109,0,0,0,1,100,97,10]))

    def test_list(self):
        self.assertEqual(earl.pack([1,2,3]), bytes([131,108,0,0,0,3,97,1,97,2,97,3,106]))

    def test_nil(self):
        self.assertEqual(earl.pack([]), bytes([131,106]))


class TestEarlUnpacking(unittest.TestCase):
    def test_smallint(self):
        self.assertEqual(earl.unpack(bytes([131,97,234])), 234)

    def test_bigint(self):
        self.assertEqual(earl.unpack(bytes([131,98,0,0,214,216])), 55000)

    def test_floats(self):
        self.assertEqual(earl.unpack(bytes([131,70,64,108,42,225,71,174,20,123])), 225.34)

    def test_stringext(self):
        self.assertEqual(earl.unpack(bytes([131,107,0,3,1,2,3])), bytes([1,2,3]))

    def test_list(self):
        self.assertEqual(earl.unpack(bytes([131,108,0,0,0,3,97,1,97,2,97,3,106])), [1,2,3])

    def test_map(self):
        self.assertEqual(earl.unpack(bytes([131,116,0,0,0,1,100,0,1,97,97,150])), {'a': 150})

    def test_nil(self):
        self.assertEqual(earl.unpack(bytes([131,106])), [])

    def test_atom(self):
        self.assertEqual(earl.unpack(bytes([131,100,0,5,104,101,108,108,111])), "hello")

    def test_utf8(self):
        self.assertEqual(earl.unpack(bytes([131,107,0,6,233,153,176,233,153,189]), encoding="utf8"), "陰陽")

if __name__ == "__main__":
    unittest.main()
