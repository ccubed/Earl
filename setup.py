from setuptools import setup, Extension

module1 = Extension('earl', sources = ['earl.cpp'])

setup (
	name = "Earl",
	version = "1.0.9",
	description = "Earl, the fanciest External Term Format packer and unpacker available for Python.",
	ext_modules = [module1])
