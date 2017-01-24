from setuptools import setup, Extension

module1 = Extension('earl', sources = ['earl.cpp'])

setup (
	name = "Earl",
	version = "1.4.1",
	description = "Earl, the fanciest External Term Format packer and unpacker available for Python.",
	ext_modules = [module1],
	url="https://github.com/ccubed/Earl",
	author="Charles Click",
	author_email="CharlesClick@vertinext.com",
	license="MIT",
	keywords="Erlang ETF External Term Format"
)
