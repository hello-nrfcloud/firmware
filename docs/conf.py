# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

from pathlib import Path
import sys

project = 'Thingy:91 X Out-of-Box firmware'
copyright = '2024, Nordic Semiconductor'
author = 'Nordic Semiconductor'
release = '1.0.0'

# Paths

DOC_BASE = Path(__file__).absolute()

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

sys.path.insert(0, str("_extensions"))

extensions = [
    'breathe',
    'sphinxcontrib.mscgen',
    'sphinx_tabs.tabs',
    'sphinx_togglebutton',
]

templates_path = ['_templates']
exclude_patterns = ['_build_sphinx', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_ncs_theme'

# Include following files at the end of each .rst file.
rst_epilog = """
.. include:: /links.txt
"""
