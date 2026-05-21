function NCS () {
  "use strict";

  let state = {};

  // XXX: do not remove the trailing '/'
  const STABLE_VERSION_RE = /^(\d+\.)+\d+$/;
  const DEV_VERSION_RE = /^(\d+\.)+\d+-[a-z0-9]+$/;
  const LOCALHOST_RE = /^(localhost)|(0\.0\.0\.0)|((\d{1,3}\.){3}\d{1,3}):\d{4,5}/

  /*
   * Allow running from localhost; local build can be served with:
   * python -m http.server
   */
  state.updateLocations = function(){
    const host = window.location.host;
    if (LOCALHOST_RE.test(host)) {
      this.url_prefix = "/";
      this.url_root = window.location.protocol + "//" + host + "/";
      this.version_data_url = this.url_root + "versions.json";
    } else {
      // Detect project prefix from path (e.g. /ncs/, /ncs-bm/, /addons/)
      // by finding the first segment that is a version or "latest".
      const segments = window.location.pathname.split("/").filter(Boolean);
      let prefix = "/";
      for (let i = 0; i < segments.length; i++) {
        if (segments[i] === "latest" || STABLE_VERSION_RE.test(segments[i]) ||
            DEV_VERSION_RE.test(segments[i])) {
          break;
        }
        prefix += segments[i] + "/";
      }
      this.url_prefix = prefix;
      this.url_root = window.location.protocol + "//" + host + prefix;
      this.version_data_url = this.url_root + "latest/versions.json";
    }
  };

  /*
   * Infer the currently running version of the documentation
   */
  state.findCurrentVersion = function() {
    const path = window.location.pathname;
    if (path.startsWith(this.url_prefix)) {
      const prefix_len = this.url_prefix.length;
      window.NCS.current_version_text = path.slice(prefix_len).split("/")[0];
      if (window.NCS.current_version_text === "latest") {
        window.NCS.current_version = window.NCS.versions[0];
      } else {
        window.NCS.current_version = window.NCS.current_version_text;
      }
    } else {
      window.NCS.current_version_text = "latest";
      window.NCS.current_version = window.NCS.versions[0];
    }
  };

  /*
   * Infer the current page being browsed, stripped from any fixed and
   * versioned prefix; it'll be used to jump to the same page in another
   * docset version.
   */
  state.findCurrentPage = function() {
    const path = window.location.pathname;
    const version_prefix = window.NCS.current_version_text + "/";
    if (path.startsWith(this.url_prefix)) {
      const prefix_len = this.url_prefix.length;
      let new_page = path.slice(prefix_len);
      if (new_page.startsWith(version_prefix)) {
        new_page = new_page.slice(version_prefix.length);
      }
      window.NCS.current_page = new_page;
    } else {
      window.NCS.current_page = "nrf/index.html";
    }
  };

  /*
   * Updates the dropbox of nRF Connect SDK versions displayed below the
   * current development version, and links to the same page currently being
   * browsed in those earlier releases.
   */
  state.updateVersionDropDown = function() {
    const ncs = window.NCS;

    // Update dropdown text
    if (LOCALHOST_RE.test(window.location.host)) {
      $("#ncsversion").text("(local)");
    } else {
      $("#ncsversion").text(`v${ncs.current_version}`);
    }

    // Update dropdown content
    $.each(ncs.versions, function(i, v) {
      if (v !== ncs.current_version) {
        let rv = i === 0 ? "latest" : v;
        let link = `<a class="dropdown-item" href=${ncs.url_root + rv}/${ncs.current_page}>${rv}</a>`;
        $("div.dropdown-menu").append(link);
      }
    });
  };

  /*
   * Display a message at the top of the page to inform the user that the
   * version currently being browsed is not the latest.
   */
  state.showVersion = function() {
    const ncs = window.NCS;
    const last_version = ncs.versions[1];
    const path_suffix = "/" +  ncs.current_page;
    const last_release_url = ncs.url_root + last_version + path_suffix;
    const latest_release_url = ncs.url_root + "latest" + path_suffix;

    const SWITCH_MSG = "You might want to switch to the documentation for " +
      "the <a href='" + last_release_url + "'>" + last_version +
      "</a> release or the <a href='" + latest_release_url + "'>current " +
      "state of development</a>."

    const OLD_RELEASE_MSG =
      "You are looking at an older version of the documentation. " + SWITCH_MSG;

    const DEV_RELEASE_MSG =
      "You are looking at the documentation for a development tag. " + SWITCH_MSG;

    const LAST_RELEASE_MSG =
      "You are looking at the documentation for the latest official release.";

    if (ncs.current_version === last_version) {
      $("div.announcement").html(LAST_RELEASE_MSG);
      $("div.announcement").show();
    } else if (DEV_VERSION_RE.test(ncs.current_version)) {
      $("div.announcement").html(DEV_RELEASE_MSG);
      $("div.announcement").show();
    } else if (ncs.versions.includes(ncs.current_version) &&
               ncs.current_version !== ncs.versions[0]) {
      $("div.announcement").html(OLD_RELEASE_MSG);
      $("div.announcement").show();
    }
  };

  state.updatePage = function() {
    let ncs = window.NCS;
    ncs.findCurrentVersion();
    ncs.findCurrentPage();
    ncs.updateVersionDropDown();
  /*ncs.showVersion();*/
  };

  const NCS_SESSION_KEY = "ncs";

  /*
   * Load a versions.json from the session cache if available
   */
  state.loadVersions = function() {
    let versions_data = window.sessionStorage.getItem(NCS_SESSION_KEY);
    if (versions_data) {
      window.NCS.versions = JSON.parse(versions_data);
      return true;
    }
    return false;
  }

  /*
   * Update the session cache with a new versions.json
   */
  state.saveVersions = function(versions_data) {
    const session_value = JSON.stringify(versions_data);
    window.sessionStorage.setItem(NCS_SESSION_KEY, session_value);
    window.NCS.versions = versions_data;
  }

  /*
   * When the "Hide Search Matches" (from Sphinx's doctools) link is clicked,
   * rewrite the URL to remove the search term.
   */
  state.hideSearchMatches = function() {
    $('.highlight-link > a').on('click', function(){
      // Remove any ?highlight=* search querystring element
      window.location.assign(
        window.location.href.replace(/[?]highlight=[^#]*/, '')
      );
    });
  }

  return state;
};

if (typeof window !== 'undefined') {
  window.NCS = NCS();
}
/**
 * Default heading above the right-hand local ToC when the page has no
 * .. contents:: caption (no p.topic-title). Matches docutils markup for styling.
 */
function initNcsLocalTocHeading() {
  var TITLE = 'On This Topic';
  var $roots = $('.contents.local').add('nav.contents.local');
  $roots.each(function () {
    var $root = $(this);
    if ($root.children('p.topic-title').length) {
      return;
    }
    if ($root.find('> .ncs-local-toc-heading').length) {
      return;
    }
    var $title = $('<p class="topic-title ncs-local-toc-heading"></p>')
      .text(TITLE)
      .attr('role', 'heading')
      .attr('aria-level', '3');
    $root.prepend($title);
  });
}
/**
 * Right-hand local ToC: inject .toctree-expand buttons.
 * Toggle is independent of the sidebar: several branches can stay open.
 */
function initNcsLocalTocExpand() {
  function toggleNcsLocalTocBranch($link) {
    var $li = $link.closest('li');
    if (!$li.children('ul').length) {
      return;
    }
    if ($li.hasClass('current')) {
      $li.find('li.current').removeClass('current').attr('aria-expanded', 'false');
    }
    $li.toggleClass('current');
    $li.attr('aria-expanded', $li.hasClass('current') ? 'true' : 'false');
  }

  function initLocalTocExpand(rootSelector) {
    $(rootSelector).each(function () {
      var $root = $(this);
      $root.find('ul').each(function () {
        var $ul = $(this);
        if (!$ul.parent().is('li')) {
          return;
        }
        var $link = $ul.siblings('a').first();
        if (!$link.length) {
          $link = $ul.siblings('p').children('a').first();
        }
        if (!$link.length || $link.find('.toctree-expand').length) {
          return;
        }
        var expandBtn = $(
          '<button type="button" class="toctree-expand" title="Open/close menu" aria-expanded="false"></button>'
        );
        expandBtn.on('click', function (ev) {
          toggleNcsLocalTocBranch($link);
          expandBtn.attr(
            'aria-expanded',
            $link.closest('li').hasClass('current') ? 'true' : 'false'
          );
          ev.stopPropagation();
          ev.preventDefault();
          return false;
        });
        $link.append(expandBtn);
      });
    });
  }
  initLocalTocExpand('.contents.local');
  initLocalTocExpand('nav.contents.local');
}
/**
 * Header toolbar: expand/collapse all branches.
 * Runs after initNcsLocalTocExpand so .toctree-expand exists for aria sync.
 */
function initNcsLocalTocToolbar() {
  function syncExpandButtons($root) {
    $root.find('.toctree-expand').each(function () {
      var open = $(this).closest('li').hasClass('current');
      $(this).attr('aria-expanded', open ? 'true' : 'false');
    });
  }

  function mount($root) {
    if ($root.children('.ncs-local-toc-header').length) {
      return;
    }
    var $title = $root.children('.topic-title').first();
    if (!$title.length) {
      return;
    }

    var $header = $('<div class="ncs-local-toc-header"></div>');
    var $toolbar = $(
      '<div class="ncs-local-toc-toolbar" role="toolbar" aria-label="On this topic"></div>'
    );

    var $expandAll = $(
      '<button type="button" class="ncs-local-toc-tool ncs-local-toc-expand-all"></button>'
    )
      .attr('title', 'Expand all')
      .attr('aria-label', 'Expand all sections')
      .html(
        '<span class="ncs-local-toc-guillemet-stack" aria-hidden="true">' +
        '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--up">\u203A</span>' +
        '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--down">\u203A</span>' +
        '</span>'
      );

    var $collapseAll = $(
      '<button type="button" class="ncs-local-toc-tool ncs-local-toc-collapse-all"></button>'
    )
      .attr('title', 'Collapse all')
      .attr('aria-label', 'Collapse all sections')
      .html(
        '<span class="ncs-local-toc-guillemet-stack" aria-hidden="true">' +
        '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--down">\u203A</span>' +
        '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--up">\u203A</span>' +
        '</span>'
      );
    var $g1 = $('<div class="ncs-local-toc-toolbar-group"></div>').append($expandAll);
    var $g2 = $(
      '<div class="ncs-local-toc-toolbar-group ncs-local-toc-toolbar-group--divider"></div>'
    ).append($collapseAll);
    $toolbar.append($g1, $g2);
    $header.append($title.detach(), $toolbar);
    $root.prepend($header);

    $expandAll.on('click', function () {
      $root
        .find('li')
        .filter(function () {
          return $(this).children('ul').length > 0;
        })
        .addClass('current')
        .attr('aria-expanded', 'true');
      syncExpandButtons($root);
    });

    $collapseAll.on('click', function () {
      $root.find('li.current').removeClass('current').attr('aria-expanded', 'false');
      syncExpandButtons($root);
    });
  }
  $('.contents.local').add('nav.contents.local').each(function () {
    mount($(this));
  });
}
/**
 * Left sidebar global ToC: expand/collapse all (same controls as right local ToC).
 * Inserts a bar above .wy-menu-vertical; uses li.current + .toctree-expand like theme.js.
 */
function initNcsSidebarTocToolbar() {
  var $menu = $('.wy-menu-vertical').first();
  if (!$menu.length || $menu.prev('.ncs-sidebar-toc-header').length) {
    return;
  }

  function syncExpandButtons() {
    $menu.find('.toctree-expand').each(function () {
      var open = $(this).closest('li').hasClass('current');
      $(this).attr('aria-expanded', open ? 'true' : 'false');
    });
  }

  function currentPageLink() {
    var path = window.location.pathname.replace(/\/+$/, '') || '/';
    return $menu.find('a[href]').filter(function () {
      try {
        var p = new URL(this.href, window.location.href).pathname.replace(/\/+$/, '') || '/';
        return p === path;
      } catch (e) {
        return false;
      }
    }).first();
  }

  var $header = $('<div class="ncs-sidebar-toc-header"></div>');
  var $toolbar = $(
    '<div class="ncs-local-toc-toolbar" role="toolbar" aria-label="Navigation"></div>'
  );

 
  var $expandAll = $(
    '<button type="button" class="ncs-sidebar-toc-btn ncs-sidebar-toc-btn--expand"></button>'
  )
    .attr('title', 'Expand all')
    .attr('aria-label', 'Expand all sections')
    .html(
      '<span class="ncs-local-toc-guillemet-stack" aria-hidden="true">' +
      '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--up">\u203A</span>' +
      '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--down">\u203A</span>' +
      '</span>' +
      '<span class="ncs-sidebar-toc-btn__label">Expand</span>'
    );

  var $collapseAll = $(
    '<button type="button" class="ncs-sidebar-toc-btn ncs-sidebar-toc-btn--collapse"></button>'
  )
    .attr('title', 'Collapse all')
    .attr('aria-label', 'Collapse all sections')
    .html(
      '<span class="ncs-local-toc-guillemet-stack" aria-hidden="true">' +
      '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--down">\u203A</span>' +
      '<span class="ncs-local-toc-guillemet ncs-local-toc-guillemet--up">\u203A</span>' +
      '</span>' +
      '<span class="ncs-sidebar-toc-btn__label">Collapse</span>'
    );

  var $g1 = $('<div class="ncs-local-toc-toolbar-group"></div>').append($expandAll);
  var $g2 = $(
    '<div class="ncs-local-toc-toolbar-group ncs-local-toc-toolbar-group--divider"></div>'
  ).append($collapseAll);
  $toolbar.append($g1, $g2);
  $header.append($toolbar);
  $menu.before($header);

  $expandAll.on('click', function () {
    $menu
      .find('li')
      .filter(function () {
        return $(this).children('ul').length > 0;
      })
      .addClass('current')
      .attr('aria-expanded', 'true');
    // Top-level leaf ToC rows (no nested <ul>): same .current styling as branches
    $menu
      .children('ul')
      .children('li')
      .filter(function () {
        return $(this).children('ul').length === 0;
      })
      .addClass('current')
      .attr('aria-expanded', 'true');
    syncExpandButtons();
  });

  $collapseAll.on('click', function () {
    $menu.find('li.current').removeClass('current').attr('aria-expanded', 'false');
    var $link = currentPageLink();
    if ($link.length) {
      $link.parents('.wy-menu-vertical li').addClass('current').attr('aria-expanded', 'true');
    }
    syncExpandButtons();
  });
}
/**
 * Highlight local ToC link for the section nearest the top of the viewport
 * (scroll spy). Uses class ncs-toc-active — not li.current (collapse state).
 */
function initNcsLocalTocScrollSpy() {
  var ACTIVE = 'ncs-toc-active';
  var $roots = $('.contents.local').add('nav.contents.local');
  if (!$roots.length) {
    return;
  }

  var sections = [];
  var byId = {};

  $roots.find('a[href^="#"]').each(function () {
    var href = this.getAttribute('href');
    if (!href || href === '#') {
      return;
    }
    var id = decodeURIComponent(href.slice(1));
    var target = document.getElementById(id);
    if (!target) {
      return;
    }
    if (!byId[id]) {
      byId[id] = { id: id, el: target, links: [] };
      sections.push(byId[id]);
    }
    byId[id].links.push(this);
  });

  if (!sections.length) {
    return;
  }

  function docTop(el) {
    var r = el.getBoundingClientRect();
    return r.top + window.pageYOffset;
  }

  sections.sort(function (a, b) {
    return docTop(a.el) - docTop(b.el);
  });

  function headerOffset() {
    var v = getComputedStyle(document.documentElement).getPropertyValue('--header-top-height');
    var n = parseInt(v, 10);
    return (24);
  }

  var raf = null;
  function update() {
    raf = null;
    var y = window.pageYOffset + headerOffset();
    var currentId = null;
    var i;
    for (i = sections.length - 1; i >= 0; i--) {
      if (docTop(sections[i].el) <= y) {
        currentId = sections[i].id;
        break;
      }
    }

    $roots.find('a.' + ACTIVE).removeClass(ACTIVE);
    if (currentId !== null) {
      for (i = 0; i < sections.length; i++) {
        if (sections[i].id === currentId) {
          $(sections[i].links).addClass(ACTIVE);
          break;
        }
      }
    }
  }

  function onScrollOrResize() {
    if (raf !== null) {
      return;
    }
    raf = window.requestAnimationFrame(update);
  }

  window.addEventListener('scroll', onScrollOrResize, { passive: true });
  window.addEventListener('resize', onScrollOrResize);
  window.addEventListener('hashchange', update);
  update();
}
$(document).ready(function(){
  initNcsLocalTocHeading();
  initNcsLocalTocExpand();
  initNcsLocalTocToolbar();
  initNcsSidebarTocToolbar();
  initNcsLocalTocScrollSpy();
  /* Preserve sidebar scroll — same-page expand/collapse + cross-page navigation */
  var $sideScroll = $('.wy-side-scroll');
  var _sideScrollLockUntil = 0;
  var _sideScrollSavedTop = 0;

  // On load: restore previous page position and lock for 500ms so nothing overrides it.
  (function () {
    var saved = sessionStorage.getItem('ncs-sidebar-scroll');
    if (saved !== null) {
      _sideScrollSavedTop  = parseInt(saved, 10);
      _sideScrollLockUntil = Date.now() + 500;
      $sideScroll[0].scrollTop = _sideScrollSavedTop;
    }
  }());

  // Capture phase — fires BEFORE theme.js stopPropagation on expand buttons.
  document.addEventListener('click', function (e) {
    if (e.target.closest('.wy-menu-vertical')) {
      _sideScrollSavedTop  = $sideScroll[0].scrollTop;
      _sideScrollLockUntil = Date.now() + 300;
      // Save immediately so it is fresh when the next page loads.
      sessionStorage.setItem('ncs-sidebar-scroll', _sideScrollSavedTop);
    }
  }, true);

  // Scroll listener — enforce lock, then persist normal user scroll.
  $sideScroll[0].addEventListener('scroll', function () {
    if (Date.now() < _sideScrollLockUntil) {
      $sideScroll[0].scrollTop = _sideScrollSavedTop;
    } else {
      sessionStorage.setItem('ncs-sidebar-scroll', $sideScroll[0].scrollTop);
    }
  });

  // Prevent full-page reload when clicking a link already on the current page.
  $(document).on('click', '.wy-menu-vertical a', function (e) {
    var a = document.createElement('a');
    a.href = $(this).attr('href') || '';
    if (a.pathname === window.location.pathname) {
      e.preventDefault();
    }
  });
  window.NCS.updateLocations();
  window.NCS.hideSearchMatches();

  if (window.NCS.loadVersions()) {
    window.NCS.updatePage();
  } else {
    /* Get versions file from remote server. */
    $.getJSON(window.NCS.version_data_url, function(json_data) {
      window.NCS.saveVersions(json_data);
      window.NCS.updatePage();
    });
  }
});
