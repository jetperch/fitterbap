#!/usr/bin/env python3
# Copyright 2018-2021 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Fitterbap python setuptools module.

See:
https://packaging.python.org/en/latest/distributing.html
https://github.com/pypa/sampleproject
"""

# Always prefer setuptools over distutils
import setuptools
import setuptools.dist
import distutils.cmd
from distutils.errors import DistutilsExecError
import os
import sys

setuptools.dist.Distribution().fetch_build_eggs(['Cython>=0.20.1', 'numpy>=1.20', 'wheel'])

import numpy as np


MYPATH = os.path.dirname(os.path.abspath(__file__))
VERSION_PATH = os.path.join(MYPATH, 'pyfitterbap', 'version.py')
C_INC_PATH = os.path.join(MYPATH, 'include')
C_INC2_PATH = os.path.join(MYPATH, 'pyfitterbap', 'include')
C_INC3_PATH = os.path.join(MYPATH, 'third-party', 'tinyprintf')


try:
    from Cython.Build import cythonize
    USE_CYTHON = os.path.isfile(os.path.join(MYPATH, 'pyfitterbap', 'comm', 'comm.pyx'))
except ImportError:
    USE_CYTHON = False


about = {}
with open(VERSION_PATH, 'r', encoding='utf-8') as f:
    exec(f.read(), about)


C_INCS = [C_INC_PATH, C_INC2_PATH, C_INC3_PATH, np.get_include()]

if sys.platform.startswith('win'):
    platform_sources = [
        'src/host/win/comm.c',
        'src/host/win/error.c',
        'src/host/win/platform.c',
        'src/host/win/uart.c',
        'src/host/platform_alloc.c',
    ]
elif sys.platform.startswith('linux'):
    platform_sources = [
        'src/host/linux/platform.c'
        'src/host/platform_alloc.c',
    ]
elif sys.platform.startswith('darwin'):  # macos
    platform_sources = []
else:
    platform_sources = []


ext = '.pyx' if USE_CYTHON else '.c'
extensions = [
    setuptools.Extension('pyfitterbap.comm.comm',
        sources=[
            'pyfitterbap/comm/comm' + ext,
            'src/event_manager.c',
            'src/collections/ring_buffer_msg.c',
            'src/comm/data_link.c',
            'src/comm/framer.c',
            'src/comm/log_port.c',
            'src/comm/port.c',
            'src/comm/port0.c',
            'src/comm/pubsub_port.c',
            'src/comm/stack.c',
            'src/comm/timesync.c',
            'src/comm/transport.c',
            'src/crc.c',
            'src/cstr.c',
            'src/fsm.c',
            'src/log.c',
            'src/logh.c',
            'src/pubsub.c',
            'src/topic.c',
            'src/topic_list.c',
            'src/union.c',
            'third-party/tinyprintf/tinyprintf.c'
        ] + platform_sources,
        include_dirs=C_INCS,
    ),
    setuptools.Extension('pyfitterbap.crc',
         sources=[
             'pyfitterbap/crc' + ext,
             'src/crc.c',
             ],
         include_dirs=C_INCS,
    ),
]

if USE_CYTHON:
    from Cython.Build import cythonize
    extensions = cythonize(extensions, compiler_directives={'language_level': '3'})  # , annotate=True)


# Get the long description from the README file
with open(os.path.join(MYPATH, 'README.md'), 'r', encoding='utf-8') as f:
    long_description = f.read()


if sys.platform.startswith('win'):
    PLATFORM_INSTALL_REQUIRES = ['pypiwin32>=223']
else:
    PLATFORM_INSTALL_REQUIRES = []


class CustomBuildDocs(distutils.cmd.Command):
    """Custom command to build docs locally."""

    description = 'Build docs.'
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        # sphinx-build -b html docs build\docs_html
        # defer import so not all setups require sphinx
        from sphinx.application import Sphinx
        from sphinx.util.console import nocolor, color_terminal
        nocolor()
        source_dir = os.path.join(MYPATH, 'docs')
        target_dir = os.path.join(MYPATH, 'build', 'docs_html')
        doctree_dir = os.path.join(target_dir, '.doctree')
        app = Sphinx(source_dir, source_dir, target_dir, doctree_dir, 'html')
        app.build()
        if app.statuscode:
            raise DistutilsExecError(
                'caused by %s builder.' % app.builder.name)


setuptools.setup(
    name=about['__title__'],
    version=about['__version__'],
    description=about['__description__'],
    long_description=long_description,
    long_description_content_type='text/markdown',
    url=about['__url__'],
    author=about['__author__'],
    author_email=about['__author_email__'],
    license=about['__license__'],

    # Classifiers help users find your project by categorizing it.
    #
    # For a list of valid classifiers, see https://pypi.org/classifiers/
    classifiers=[  # Optional
        'Development Status :: 4 - Beta',

        # Indicate who your project is intended for
        'Intended Audience :: Developers',
        'Intended Audience :: End Users/Desktop',
        'Intended Audience :: Science/Research',

        # Pick your license as you wish
        'License :: OSI Approved :: Apache Software License',

        # Operating systems
        'Operating System :: Microsoft :: Windows :: Windows 10',
        'Operating System :: Microsoft :: Windows :: Windows 8.1',
        'Operating System :: Microsoft :: Windows :: Windows 8',
        'Operating System :: Microsoft :: Windows :: Windows 7',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: POSIX :: Linux',

        # Supported Python versions
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: Implementation :: CPython',
        
        # Topics
        'Topic :: Scientific/Engineering',
        'Topic :: Software Development :: Embedded Systems',
        'Topic :: Software Development :: Testing',
        'Topic :: Utilities',
    ],

    keywords='fitterbap pubsub uart',

    packages=setuptools.find_packages(exclude=['native', 'docs', 'test', 'dist', 'build']),
    include_package_data=True,
    ext_modules=extensions,
    cmdclass={
        'docs': CustomBuildDocs,
    },
    include_dirs=[],
    
    # See https://packaging.python.org/guides/distributing-packages-using-setuptools/#python-requires
    python_requires='~=3.8',

    setup_requires=[
        # https://developercommunity.visualstudio.com/content/problem/1207405/fmod-after-an-update-to-windows-2004-is-causing-a.html
        'numpy>=1.20',
        'Cython>=0.29.3',
    ],

    # See https://packaging.python.org/en/latest/requirements.html
    install_requires=[
        'numpy>=1.20',
        'psutil>=1.12',
        'pyqtgraph',
        'pyserial',
        'PySide6>=6.1.0',
        'python-dateutil>=2.7.3',
        'pyqtgraph>=0.12',
    ] + PLATFORM_INSTALL_REQUIRES,

    extras_require={
        'dev': ['check-manifest', 'coverage', 'Cython', 'wheel', 'sphinx', 'm2r'],
    },   

    entry_points={
        'console_scripts': [
            'fitterbap=pyfitterbap.__main__:run',
        ],
    },
    
    project_urls={
        'Bug Reports': 'https://github.com/jetperch/fitterbap/issues',
        'Funding': 'https://www.joulescope.com',
        'Twitter': 'https://twitter.com/joulescope',
        'Source': 'https://github.com/jetperch/fitterbap',
    },
)
