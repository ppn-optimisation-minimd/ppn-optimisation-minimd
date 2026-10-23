// Handle page tooltips (but not chart.js tooltips)
$( function() {
  $( document ).tooltip({
    content: function() {
      if (this.classList.contains ("tooltip-maqao")
      ||  this.classList.contains ("tooltip-maqao-button")) {
        return tooltips_texts[$(this).attr('tooltip_id')];
      }
    },
    position: {
      my: "center bottom-20",
        at: "center top",
        using: function( position, feedback ) {
          $( this ).css( position );
          $( "<div>" )
            .addClass( "arrow-tt" )
            .addClass( feedback.vertical )
            .addClass( feedback.horizontal )
            .appendTo( this );
        }
      }
  });
});


// Code from https://www.htmlgoodies.com/beyond/javascript/article.php/3724571/Using-Multiple-JavaScript-Onload-Functions.htm
// Allow to declare multiple onLoad functions
function addLoadEvent(func) {
  var oldonload = window.onload;
  if (typeof window.onload != 'function') {
    window.onload = func;
  } else {
    window.onload = function() {
      if (oldonload) {
        oldonload();
      }
      func();
    }
  }
}


// Get an associative array of parameters stored in page URL
function getQueryStringArray(){
  var assoc=[];
  var items = window.location.search.substring(1).split('&');
  for(var j = 0; j < items.length; j++) {
    var a = items[j].split('='); assoc[a[0] ]  = a[1];
  }
  return assoc;
}


// Overload localStorage.setItem function to easily disabled it using a global variable
let __is_localStorage_updated = true;
function updateLocalStorage (__key__, __value__) {
  if (__is_localStorage_updated == true) {
    localStorage.setItem(__key__, __value__);
  }
}


// Disable localStorage update if updls (UPDate LocalStorage) is set to false in URL
addLoadEvent(function() {
  var qs = getQueryStringArray();
  if (qs.updls && qs.updls !== "undefined" && qs.updls == "false") {
    __is_localStorage_updated = false;
  }
});


// Load a CSS stylesheet
function loadCSS(filename){
  var file = document.createElement("link");
  file.setAttribute("rel", "stylesheet");
  file.setAttribute("type", "text/css");
  file.setAttribute("href", filename);
  document.head.appendChild(file);
}


// ----------------------------------------------------------------------------
// Variables and functions used to handle tooltips in chart.js charts
let __chartJS_overing        = false;  // Set to true when the chart.js tooltip must be displayed
let __chartJS_overing_labels = false;  // Set to true when an X axis label is hovered


// Return the overed label in chart.js axes labels
const findLabel = (labels, evt) => {
  let found = false;
  let res = null;
  labels.forEach(l => {
    l.labels.forEach(label => {
      if (evt.x > label.x && evt.x < label.x2 && evt.y > label.y && evt.y < label.y2) {
        res = label;
        found = true;
      }
    });
  });
  return [found, res];
};


// Return collision boxes of chart.js axes labels
const getLabelHitboxes = (scales) =>
  Object.values(scales).map((s) => {
    return {
      scaleId: s.id,
      labels: s._labelItems.map((labelItem, i) => {
        if (s.id === "y") {
          return {
            x: labelItem.options.translation[0] - s._labelSizes.widths[i],
            x2: labelItem.options.translation[0],
            y: labelItem.options.translation[1] - s._labelSizes.heights[i] / 2,
            y2: labelItem.options.translation[1] + s._labelSizes.heights[i] / 2,
            label: labelItem.label,
            index: i,
            axis: "y",
          }
        } else {
          return {
            x: labelItem.options.translation[0] - s._labelSizes.widths[i] / 2,
            x2: labelItem.options.translation[0] + s._labelSizes.widths[i] / 2,
            y: labelItem.options.translation[1],
            y2: labelItem.options.translation[1] + s._labelSizes.heights[i],
            label: labelItem.label,
            index: i,
            axis: "x",
          }
        }
      }),
    }
  })


// Based on a max size, shrink __data__[i] elements and return a same size table
// with full size strings, used as tooltips
// If __max_size__ > 0, remove string ends, if __max_size__ < 0, remove string beginnings
function __extract_tooltips_from_legend (__data__, __max_size__) {
  let tooltips = [];
  for (let i = 0; i < __data__.length; i++) {
    let entry = __data__[i];
    if (__max_size__ > 0
    &&  entry.length > __max_size__) {
      tooltips.push (entry);
      __data__[i] = entry.substring (0, __max_size__)+"...";
    }
    else if (__max_size__ < 0
    && entry.length > (- __max_size__)) {
      tooltips.push (entry);
      __data__[i] = "..."+entry.substring (entry.length + __max_size__);
    }
  }
  return tooltips;
}


// Show the tooltip and set its content
function __show_chartjs_tooltip (__tts__, __index__, __tooltip__, __event__) {
  __tooltip__.innerHTML = __tts__[__index__];
  __tooltip__.style.display = "block";
  if (__event__.native.pageX + __tooltip__.clientWidth <= window.innerWidth) {
    __tooltip__.style.left = (__event__.native.pageX + 15) + "px";
  }
  else {
    __tooltip__.style.left = (window.innerWidth - __tooltip__.clientWidth)+"px";
  }
  if (__event__.native.pageY + __tooltip__.clientHeight <= window.innerHeight) {
    __tooltip__.style.top = __event__.native.pageY + "px";
  }
  else {
    __tooltip__.style.top = (window.innerHeight - __tooltip__.clientHeight)+"px";
  }
  __chartJS_overing = true;
}


// Show the tooltip and remove its content
function __hide_chartjs_tooltip (__tooltip__) {
  __tooltip__.innerHTML = "";
  __tooltip__.style.display = "none";
  __chartJS_overing = false;
}



// ----------------------------------------------------------------------------
// Functions used to extract tables as CSV
async function downloadFileAsCSV(__table__) {
  if ("showSaveFilePicker" in self) {
    const pickerOpts = {
      types: [
        {
          description: "CSV File",
          accept: {"text/plain": [".csv"]},
        },
      ],
      excludeAcceptAllOption: true,
      multiple: false,
      startIn: "downloads",
    };
    // Select the file
    let newHandle;
    try {
       newHandle = await window.showSaveFilePicker(pickerOpts);
    }
    catch (error) {
       return ;
    }
    // Open it, write it, then close it
    const writableStream = await newHandle.createWritable();
    for (var i = 0, row; row = __table__.rows[i]; i++) {
      let str_row = "";
      for (var j = 0, col; col = row.cells[j]; j++) {
        let clean_text = col.innerText.replace (/^[^a-zA-Z0-9_]+/, "");
        clean_text =        clean_text.replace (/\+$/, "");
        str_row += clean_text + ";";
      }
      await writableStream.write(str_row+"\n");
    }
    await writableStream.close();
  }
  else {
    window.alert ("feature not available for this web browser");
  }
}


function __disable_CSV_generator_buttons (__ids__){
  if ((!("showSaveFilePicker" in self))
  &&  __ids__) {
    for (var i = 0, bid; bid = __ids__[i]; i++) {
      let button = document.getElementById (bid);
      if (button) {
        button.style.display = "none";
      }
    }
  }
}


// ----------------------------------------------------------------------------
function getPos (el) {
  for (var lx = 0, ly = 0;
    el != null;
    lx += el.offsetLeft, ly += el.offsetTop, el = el.offsetParent);
  return {x: lx, y: ly};
}


// ----------------------------------------------------------------------------
function _check_all_checkbox_ (obj_id, opts_ids) {
  var obj   = document.getElementById (obj_id);
  var opts  = window[opts_ids];
  var count = 0
  for (i = 0; i < opts.length; i++) {
    var obj_s = document.getElementById(opts[i]);
    if (obj_s && obj_s.checked) {
      count ++;
    }
  }
  if (count == 0) {
    obj.value = "Select all";
  }
  else if (count == opts.length) {
    obj.value = "Select none";
  }
}

// Fonction for table selector
function click_table_menu (c, obj_id, opt_ids) {
  var is_checked = document.getElementById (c).checked;
  updateLocalStorage(c, is_checked);
  $("."+c).each(function(index, element) {
    if (is_checked) {
      element.classList.remove("_tcol_hidden");
    }
    else {
      element.classList.add("_tcol_hidden");
    }
  });
  if (obj_id != undefined) {
    _check_all_checkbox_ (obj_id, opt_ids)
  }
}

function set_all_table_menu (obj, var_opt_ids) {
  var opt_ids = window[var_opt_ids];
  var is_selected = true
  if (obj.value == "Select all") {
    is_selected = false;
  }
  for (i = 0; i < opt_ids.length; i++) {
    var obj_s = document.getElementById(opt_ids[i]);
    if (obj_s && is_selected == obj_s.checked) {
      obj_s.checked = !obj_s.checked;
      click_table_menu (opt_ids[i]);
    }
  }
  if (is_selected == false) {
    obj.value = "Select none";
  } else {
    obj.value = "Select all";
  }
  updateLocalStorage(obj.id, obj.value);
}

// ----------------------------------------------------------------------------
// Script for accordion box
function _click_accordion_header(obj) {
  var content = obj.nextSibling;
  content.classList.toggle("collapsed");
  obj.classList.toggle("collapsed");
  obj.parentNode.classList.toggle("collapsed");
  // Check if the box must be openned ...
  if (obj.classList.contains ("collapsed")) {
    obj.firstChild.textContent = "\u25B6";
  }
  // or closed.
  else {
    obj.firstChild.textContent = "\u25BC";
  }

  if (typeof _onresize === "function") {
    _onresize ();
  }
}


// ----------------------------------------------------------------------------
// Script for paged table
function _click_paged_prev(obj) {
  var pages_content_id = "_paged_tables_page_" + obj.parentNode.parentNode.id.substr (obj.parentNode.parentNode.id.lastIndexOf("_") + 1);
  var pages_content = document.getElementById(pages_content_id).innerHTML;
  var max_pages = pages_content.substr (pages_content.lastIndexOf("/") + 1);
  var cur_page  = pages_content.substring (pages_content.lastIndexOf(" ") + 1, pages_content.lastIndexOf("/"));
  max_pages = Number (max_pages);
  cur_page  = Number (cur_page);
  if (cur_page > 1) {
    var div_paged = obj.parentNode.parentNode.id;
    var i;
    var x = document.getElementsByClassName(div_paged + "_" + (cur_page - 1));
    for (i = 0; i < x.length; i++) {
      x[i].classList.toggle("_paged_hidden");
    }
    x = document.getElementsByClassName(div_paged + "_" + (cur_page - 2));
    for (i = 0; i < x.length; i++) {
      x[i].classList.toggle("_paged_hidden");
    }
  document.getElementById(pages_content_id).textContent = "Page " + (cur_page - 1) + "/" + max_pages;
  }
}
function _click_paged_next(obj) {
  var pages_content_id = "_paged_tables_page_" + obj.parentNode.parentNode.id.substr (obj.parentNode.parentNode.id.lastIndexOf("_") + 1);
  var pages_content = document.getElementById(pages_content_id).innerHTML;
  var max_pages = pages_content.substr (pages_content.lastIndexOf("/") + 1);
  var cur_page  = pages_content.substring (pages_content.lastIndexOf(" ") + 1, pages_content.lastIndexOf("/"));
  max_pages = Number (max_pages);
  cur_page  = Number (cur_page);
  if (cur_page < max_pages) {
    var div_paged = obj.parentNode.parentNode.id;
    var i;
    var x = document.getElementsByClassName(div_paged + "_" + (cur_page - 1));
    for (i = 0; i < x.length; i++) {
      x[i].classList.toggle("_paged_hidden");
    }
    x = document.getElementsByClassName(div_paged + "_" + cur_page);
    for (i = 0; i < x.length; i++) {
      x[i].classList.toggle("_paged_hidden");
    }
    document.getElementById(pages_content_id).textContent = "Page " + (cur_page + 1) + "/" + max_pages;
  }
}


// ----------------------------------------------------------------------------
// Function for tree table
function _click_tree(obj) {
  if (obj.innerHTML == "\u25BA") {
    obj.textContent = "\u25BC";
    // Open childs
    var x = document.getElementsByClassName(obj.parentNode.parentNode.id);
    for (var i = 0; i < x.length; i++) {
      x[i].classList.toggle("_tree_hidden");
    }
  }
  else if (obj.innerHTML == "\u25BC") {
    obj.textContent = "\u25BA";
    // Close all childs
    var trs = document.getElementsByTagName("tr");
    var r   = obj.parentNode.parentNode.id+"_";
    var re  = new RegExp (r, "g");
    for (var i = 0; i < trs.length; i++) {
      var s = trs[i].id+""
      if (s.match(re)) {
        var o = document.getElementById(s);
        if (! o.classList.contains("_tree_hidden")) {
          o.classList.toggle("_tree_hidden");
        }
        if (o.firstChild.firstChild.textContent == "\u25BC") {
          o.firstChild.firstChild.textContent = "\u25BA";
        }
        if (o.firstChild.childNodes.length == 3) {
          o.firstChild.childNodes[2].textContent = "+";
        }
      }
    }
    if (obj.parentNode.childNodes.length == 3) {
      obj.parentNode.childNodes[2].textContent = "+";
    }
  }
}

function _click_expand (obj) {
  var trs = document.getElementsByTagName("tr");
  var r   = obj.parentNode.parentNode.id+"_";
  var re  = new RegExp (r, "g");
  // Symbole + => expand all
  if (obj.textContent == "+") {
    for (var i = 0; i < trs.length; i++) {
      var s = trs[i].id+"";
      if (s.match(re)) {
        var o = document.getElementById(s);
        if (o.classList.contains("_tree_hidden")) {
           o.classList.toggle("_tree_hidden");
        }
        if (o.firstChild.firstChild.textContent == "\u25BA") {
          o.firstChild.firstChild.textContent = "\u25BC";
        }
        if (o.firstChild.childNodes.length == 3) {
          o.firstChild.childNodes[2].textContent = "\u2010";
        }
      }
    }
    var o = obj.parentNode.parentNode;
    if (o.firstChild.firstChild.textContent == "\u25BA") {
      o.firstChild.firstChild.textContent = "\u25BC";
    }
    obj.textContent = "\u2010";
  }
  // Symbole - => collapse all
  else {
    for (var i = 0; i < trs.length; i++) {
      var s = trs[i].id+"";
      if (s.match(re)) {
        var o = document.getElementById(s);
        if (! o.classList.contains("_tree_hidden")) {
           o.classList.toggle("_tree_hidden");
        }
        if (o.firstChild.firstChild.textContent == "\u25BC") {
          o.firstChild.firstChild.textContent = "\u25BA";
        }
        if (o.firstChild.childNodes.length == 3) {
          o.firstChild.childNodes[2].textContent = "+";
        }
      }
    }
    var o = obj.parentNode.parentNode;
    if (o.firstChild.firstChild.textContent == "\u25BC") {
      o.firstChild.firstChild.textContent = "\u25BA";
    }
    obj.textContent = "+";
  }
}


// ----------------------------------------------------------------------------
// Functions for tabs
function change_tab (e, baseId) {
  // Handle tabs
  var i = 1;
  while (true) {
    var id = baseId + i;
    var t = document.getElementById (id);
    if (t == null) {
      break;
    }
    if (id == e.dataset.tab) {
      t.style.removeProperty("display");
    }
    else {
      t.style.display = "none";
    }
    i = i + 1;
  }
  // Handle headers
  $(e).siblings().each( function () {
    this.classList.remove("selected_tab");
  });
  e.classList.add("selected_tab");
}


// ----------------------------------------------------------------------------
// Default resizing function
function __maqao_legacy_onresize () {
  var obj = document.getElementById("spacer_page_title");
  if (obj) {;
    obj.style.height = (document.getElementById("page_title").clientHeight + 20)+"px";
  }
  var obj = document.getElementById("page_title");
  if (obj) {
    var elems = document.getElementsByClassName ("_first_menu_level")
    if (obj.clientHeight < 90) {
      for (var i = 0; i < elems.length; i++) {
        elems.item(i).style.borderBottomLeftRadius = "";
        elems.item(i).style.borderBottomRightRadius = "";
      }
    }
    else {
      for (var i = 0; i < elems.length; i++) {
        elems.item(i).style.borderBottomLeftRadius = "10px";
        elems.item(i).style.borderBottomRightRadius = "10px";
      }
    }
  }
}

