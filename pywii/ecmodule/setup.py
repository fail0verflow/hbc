from distutils.core import setup, Extension

module1 = Extension('_ec',
                    sources = ['_ecmodule.c'])

setup (name = 'ELT_C',
       version = '0.1',
       description = 'C ELT functions',
       ext_modules = [module1])