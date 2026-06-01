# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'hello-nrfcloud firmware'
copyright = '2026, Nordic Semiconductor'
author = 'Nordic Semiconductor'
release = '1.0.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "sphinxcontrib.jquery",
    "sphinx_copybutton",
]

templates_path = ['_templates']
exclude_patterns = ['_build_sphinx', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_ncs_theme'
html_show_sphinx = False
html_static_path = ["_static"]
html_css_files = ["hide-versions.css"]

html_theme_options = {'docsets': {},"addons_url": "https://nrfconnect.github.io/ncs-app-index/","bare_metal_url": "","ncs_url": "https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/index.html", "ncs_label": "nRF Connect SDK Docs", "logo_url": "https://nordic-semiconductor.fluidtopics.net/"}

# Include following files at the end of each .rst file.
rst_epilog = """
.. include:: /links.txt
"""
