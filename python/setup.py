from setuptools import setup

setup(
    name="cmaes",
    version="0.9.5",
    author="beniz",
    author_email="",
    description=(""),
    license="LGPLv3",
    keywords="",
    url="",
    zip_safe=False,  # the package can run out of an .egg file
    classifiers=['Intended Audience :: Science/Research',
                 'Intended Audience :: Developers',
                 'License :: OSI Approved',
                 'Programming Language :: Python',
                 'Topic :: Software Development',
                 'Topic :: Scientific/Engineering',
                 'Operating System :: Microsoft :: Windows',
                 'Operating System :: POSIX',
                 'Operating System :: Unix',
                 'Operating System :: MacOS'],
    platforms='any',
    include_package_data=True,
    package_data={"cmaes": ["*"]},
    packages=['cmaes']
)
