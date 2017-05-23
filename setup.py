from setuptools import setup, Extension

module1 = Extension('earl', sources = ['earl.cpp'])

setup (
	name = "earl-etf",
	version = "2.0.2",
	description = "Earl-etf, the fanciest External Term Format packer and unpacker available for Python.",
	ext_modules = [module1],
	url="https://github.com/ccubed/Earl",
	author="Charles Click",
	author_email="CharlesClick@vertinext.com",
	license="MIT",
	keywords="Erlang ETF External Term Format"
)
