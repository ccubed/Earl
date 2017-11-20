from setuptools import setup, Extension

module1 = Extension('earl', sources=['earl.cpp'])

setup(
    name="earl-etf",
    version="2.1.2",
    description="Earl-etf, the fanciest External Term Format packer and unpacker available for Python.",
    ext_modules=[module1],
    test_suite='unit_tests',
    python_requires='>=3',
    url="https://github.com/ccubed/Earl",
    author="Charles Click",
    author_email="CharlesClick@vertinext.com",
    license="MIT",
    keywords="Erlang ETF External Term Format",
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: Implementation :: CPython',
        'Programming Language :: Python :: 3',
    ],
)
