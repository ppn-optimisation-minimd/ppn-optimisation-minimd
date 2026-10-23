/*
TODO

*/

function FlameGraph_M () {
  var _this    = this;
  let __chart_count = 0;
  let __charts_by_ids = []; // charts objects indexed by their integer id
  let __canvas_charts = []; // charts objects indexed by their canvas id (string)

//  var is_over_event_handled = true;	// trigger used to limit event handling
  const red_colors = ["#ff0a00","#ffaf00","#ff7e00","#ff6100","#ffa500", "#ff1d00","#ff7500","#ff9200","#ff4400","#ff9b00","#ff2700","#ffb900","#ff3100","#ff4e00","#ff6b00","#ff3a00","#ff8800","#ffc200","#ff5700"];
  const blue_colors = ["#66ccff","#053a6d","#61c5f8","#0a4275","#5cbdf0","#0f497c","#57b6e9","#003366","#317caf","#53afe2","#135083","#4ea8db","#18578a","#49a0d3","#1d5f92","#3583b6","#4499cc","#226699","#3f92c5","#276da0","#3a8abd","#2c75a8"];
  const highligth_color = "#70db70";
  const PADDING_L = 10;
  const PADDING_T = 5;
  const PADDING_R = 10;
  const PADDING_B = 5;



  // ==========================================================================
  function getRandomColor (__node__, __scheme__) {
    // Return a color that is always the same from a run to another
    let colors = red_colors;
    if (__scheme__
    &&  __scheme__ == "blue") {
      colors = blue_colors;
    }

    let color = colors [__node__.index % colors.length];
    if (__node__.parent
    &&  color == __node__.parent.color) {
      color = colors [(__node__.index + 1) % colors.length];
    }
    return color;
  }



  // ==========================================================================
  function __get_data_max_depth (__node__, __depth__) {
    let max = __depth__
    if (__node__
    &&  __node__.children) {
      for (let i = 0; i < __node__.children.length; i++) {
        let tmp = __get_data_max_depth (__node__.children[i], __depth__ + 1)
        if (tmp > max) {
          max = tmp
        }
      }
    }
    return max
  }



  // ==========================================================================
  function __get_box_width (__value__, __div_size__, __factor__) {
     return ((__div_size__ * (__value__ * __factor__)) / 100);
  }



  // ==========================================================================
  // Return cursor coordinates from the bottom left corner
  function _get_coordinates_from_event (__chart__, __event__) {
    const x = __event__.offsetX;
    const y = (__chart__.size_y + PADDING_B + PADDING_T) - __event__.offsetY;

    return [x, y];
  }



  // ==========================================================================
  function _get_collided_flamme (__chart__, __x__, __y__) {
    let foundColBox = null;

    if (__x__ < PADDING_L - 1 || __x__ > __chart__.size_x + PADDING_L + 1   // Add 1 to ease user experience with collisions for first and last flames
    ||  __y__ < PADDING_B || __y__ > __chart__.size_y + PADDING_B) {
      return null ;
    }

    let depth = 0;
    if (__chart__.config
    &&  __chart__.config.view == "top-down") {
      depth = Math.trunc ((__chart__.size_y - __y__ + PADDING_B) / __chart__.flame_height);
    }
    else {
      depth = Math.trunc ((__y__ - PADDING_B) / __chart__.flame_height);
    }

    // iterate over flames at given depth to find the pointed one
    if (__chart__.collision_boxes
    &&  __chart__.collision_boxes [depth]) {
      let nbColBox = __chart__.collision_boxes [depth].length;
      for (let i = 0; foundColBox == null && i < nbColBox; i++) {
        let colBox = __chart__.collision_boxes [depth][i];
        if (colBox.x <= __x__ && __x__ <= colBox.x + colBox.w) {
          foundColBox = colBox;
        }
        // Add a small error for first flame to improve user experience
        else if (i == 0
        && __x__ <= colBox.x + colBox.w) {
          foundColBox = colBox;
        }
        // Add a small error for last flame to improve user experience
        else if (i == nbColBox - 1
        && colBox.x <= __x__
        && __x__ <= colBox.x + 2) {
          foundColBox = colBox;
        }
        else if (colBox.w < 0   // May appear because of the -1px used to separate flames
        &&       i < nbColBox - 1) {
          if (colBox.x <= __x__
          &&  __x__ < __chart__.collision_boxes [depth][i + 1].x
          &&  __x__ < colBox.x + 1) {
            foundColBox = colBox;
          }
        }
      }
    }
    return foundColBox;
  }



  // ==========================================================================
  // Called when the cursor is over a drawn canvas
  // param event Parameter given by the trigger funtion
  function _handler_over_event (event) {
    const chart = __canvas_charts[event.target.id];
    if (chart.is_over_event_handled) {
      _set_sizes (chart);

      // compute depth using cursor location
      const [x, y] = _get_coordinates_from_event (chart, event);
      let foundColBox = null;

      // Check if the cursor is already in the current collided box
      if (chart.hovered_colBox
      &&  chart.hovered_colBox.x <= x && x <= chart.hovered_colBox.x + chart.hovered_colBox.w
      &&  chart.hovered_colBox.y <= (chart.size_y - y) && (chart.size_y - y) <= chart.hovered_colBox.y + chart.hovered_colBox.h) {
        foundColBox = chart.hovered_colBox;
      }
      else {
        foundColBox = _get_collided_flamme (chart, x, y);
      }

      // display the tooltip
      if (foundColBox != null) {
        chart.tooltip.innerHTML = foundColBox.label+": "+foundColBox.value.toFixed (2)+"%";

        if (event.pageX + 5 + chart.tooltip.clientWidth > window.innerWidth) {
          chart.tooltip.style.left = (window.innerWidth - chart.tooltip.clientWidth)+"px";
        }
        else {
          chart.tooltip.style.left = (event.pageX + 5)+"px";
        }

        if (event.pageY + 5 + chart.tooltip.clientHeight > window.innerHeight) {
          chart.tooltip.style.top = (window.innerHeight - chart.tooltip.clientHeight)+"px";
        }
        else {
          chart.tooltip.style.top = (event.pageY + 5)+"px";
        }

        chart.tooltip.style.top  = (event.pageY + 5)+"px";
        chart.tooltip.style.display = "block";
        document.body.style.cursor = "pointer";


        if (chart.hovered_colBox != foundColBox) {
          // Reset previous colBox style
          if (chart.hovered_colBox != null) {
            _redraw_colBox (chart, chart.hovered_colBox)
            chart.hovered_colBox = null;
          }
          // Save found colBox to reset its drawing after leaving it
          chart.hovered_colBox = foundColBox;
          _draw_flame_from_colBox (chart, foundColBox, true);
        }
      }
      else {
        chart.tooltip.style.display = "none";
        document.body.style.cursor = "auto";
        if (chart.hovered_colBox != null) {
          _redraw_colBox (chart, chart.hovered_colBox)
          chart.hovered_colBox = null;
        }
      }

      // Reset the timer to avoid too many event triggerings
      chart.is_over_event_handled = false;
      setTimeout(function() {chart.is_over_event_handled = true;}, 10);
    }
  }



  // ==========================================================================
  // Called when the cursed leave the canvas
  // param event Parameter given by the trigger funtion
  function _handler_leave_event (event) {
    const chart = __canvas_charts[event.target.id];
    if (chart) {
      _set_sizes (chart);
      chart.tooltip.hidden = true;
      chart.tooltip.style.display = "none";
      document.body.style.cursor = "auto";
      if (chart.hovered_colBox) {
        _redraw_colBox (chart, chart.hovered_colBox)
        chart.hovered_colBox = null;
      }
    }
  }



  // ==========================================================================
  function _handler_onclick (event) {
    const chart = __canvas_charts[event.target.id];
    if (chart) {
      _set_sizes (chart);
      // Click on an existing flame to enable zoom view
      const [x, y] = _get_coordinates_from_event (chart, event);
      let foundColBox = _get_collided_flamme (chart, x, y);

      if (foundColBox == null && chart.hovered_colBox) {
        foundColBox = chart.hovered_colBox;
      }

      // display the tooltip
      if (foundColBox != null) {
        // flag all ancestors for printing function
        let p = foundColBox.node.parent;
        foundColBox.node.zoom_flag = 2;    // Flame the zoom is on
        while (p) {
          p.zoom_flag = 1;                 // Ancestor flames
          p = p.parent;
        }

        // draw the zoomed chart
        chart.ctx.clearRect(0, 0, chart.size_x + PADDING_L + PADDING_R, chart.size_y + PADDING_T + PADDING_B); // Clean screen
        chart.collision_boxes = [];                                         // Reset collisions table
        _draw_flames (chart, chart.data[0], chart.width_factor, 1, 0);      // Draw
        chart.current_view = "zoomed"                                       // Set view status
        chart.hovered_colBox = null;                                        // Delete saved hovered colBox
      }
    }
  }



  // ==========================================================================
  function _handler_on_rightclick (event) {
    const chart = __canvas_charts[event.target.id];
    if (chart) {
      _set_sizes (chart);
      // Disable the event if current view is already global view
      if (chart.current_view != "global") {

        let max_depth = chart.collision_boxes.length;
        for (let depth = 0; depth < max_depth; depth++) {
          let count = chart.collision_boxes[depth].length;
          for (let i = 0; i < count; i++) {
            chart.collision_boxes[depth][i].node.zoom_flag = 0;
          }
        }

        chart.ctx.clearRect(0, 0, chart.size_x + PADDING_L + PADDING_R, chart.size_y + PADDING_T + PADDING_B); // Clean screen
        chart.collision_boxes = [];                                         // Reset collisions table
        _draw_flames (chart, chart.data[0], chart.width_factor, 1, 0);      // Draw
        chart.current_view = "global"                                       // Set view status
        chart.hovered_colBox = null;                                        // Delete saved hovered colBox
      }
    }

    // Disable default menu
    return false;
  }



  // ==========================================================================
  function _redraw_colBox (__chart__, __colBox__) {
    // Function called to remove a flame border
    // Clean a flame based on its collision box, with its left and right neigbours
    let lu_corner = __colBox__.x - 1;
    let ru_corner = __colBox__.x + __colBox__.w + 1;
    let l_colBox  = null;
    let r_colBox  = null;
    if (__colBox__.depth_index > 0) {
      l_colBox = __chart__.collision_boxes [__colBox__.depth - 1][__colBox__.depth_index - 1];
      lu_corner = l_colBox.x;
    }
    if (__colBox__.depth_index < __chart__.collision_boxes [__colBox__.depth - 1].length - 1) {
      r_colBox = __chart__.collision_boxes [__colBox__.depth - 1][__colBox__.depth_index + 1];
      ru_corner = r_colBox.x + r_colBox.w;
    }

    __chart__.ctx.clearRect(lu_corner, __colBox__.y - 1, ru_corner - lu_corner, __colBox__.h + 2)
    if (l_colBox != null) {
      _draw_flame_from_colBox (__chart__, l_colBox, false);
    }
    _draw_flame_from_colBox (__chart__, __colBox__, false);
    if (r_colBox != null) {
      _draw_flame_from_colBox (__chart__, r_colBox, false);
    }
  }



  // ==========================================================================
  function _draw_flame_text (__chart__, __colBox__) {
    const new_size = (__colBox__.w - 2) / __chart__.font_width;

    __chart__.ctx.fillStyle = "black";
    if (new_size >= __colBox__.label.length) {
       __chart__.ctx.fillText(__colBox__.label, __colBox__.x + 2, __colBox__.y + __chart__.font_height + ((__colBox__.h - __chart__.font_height) / 2) - 1);
    }
    else if (new_size >= 3) {
      let short_label = __colBox__.label.substring (0, new_size - 2)+"..";
      __chart__.ctx.fillText(short_label, __colBox__.x + 2, __colBox__.y + __chart__.font_height + ((__colBox__.h - __chart__.font_height) / 2));
    }
  }



  // ==========================================================================
  function _draw_flame_from_colBox (__chart__, __colBox__, __is_hover__) {
      __chart__.ctx.save();

     if (__colBox__.is_zoomed == true) {
       __chart__.ctx.globalAlpha = 0.5;
     }

     if (__is_hover__ == true) {
       __chart__.ctx.strokeStyle = "slategray";
       __chart__.ctx.font = "bold "+__chart__.ctx.font;
     }
     else {
       __chart__.ctx.strokeStyle = __colBox__.color;
     }

     if (__colBox__.node.sec_color) {
       __chart__.ctx.fillStyle = __colBox__.node.sec_color;
     }
     else {
       __chart__.ctx.fillStyle = __colBox__.color;
     }

     __chart__.ctx.beginPath();
     __chart__.ctx.roundRect (__colBox__.x, __colBox__.y, __colBox__.w, __colBox__.h, __chart__.flame_radius);
     __chart__.ctx.fill();
     if (__is_hover__ == true) {
       __chart__.ctx.stroke();
     }

    _draw_flame_text (__chart__, __colBox__);

     if (__colBox__.is_zoomed == true) {
       __chart__.ctx.globalAlpha = 1;
     }

    __chart__.ctx.restore();
  }



  // ==========================================================================
  function _draw_flames (__chart__, __node__, __factor__, __depth__, __offset_h__) {
    if (__node__) {
      // Set colors for the node only once
      if (!__node__.color) {
         __node__.index = __chart__.flame_count;
         __chart__.flame_count ++;
         __node__.color = getRandomColor (__node__, __chart__.config.color);
      }

      if (!__chart__.collision_boxes [__depth__ - 1]) {
        __chart__.collision_boxes [__depth__ - 1] = [];
      }

      // Define box location and size
      const colBox = {
        x: __offset_h__ + PADDING_L,
        y: __chart__.size_y + PADDING_T - ((__chart__.flame_height * __depth__)),
        w: __get_box_width (__node__.value, __chart__.size_x, __factor__) - 1,
        h: __chart__.flame_height - 1,
        label: __node__.label,
        value: __node__.value,
        color: __node__.color,
        node: __node__,
        is_zoomed: false,
        depth: __depth__,
        depth_index: __chart__.collision_boxes [__depth__ - 1].length,
      }

      if (__chart__.config
      &&  __chart__.config.view == "top-down") {
         colBox.y = ((__depth__ - 1) * __chart__.flame_height) + PADDING_T;
      }

      // Handle width when zoomed
      if (__node__.zoom_flag == 1
      ||  __node__.zoom_flag == 2) {
        const root_w = __get_box_width (__chart__.data[0].value, __chart__.size_x, __factor__) - 1;

        // Change factor for children
        if (__node__.zoom_flag == 2) {
          __factor__ = 100 / __node__.value;
        }
        colBox.w = root_w;
        if (__node__.zoom_flag == 1) {
          colBox.is_zoomed = true;
        }
      }

      // Save the box into collision boxes array
      __chart__.collision_boxes [__depth__ - 1].push (colBox);

      // Draw the flame
      _draw_flame_from_colBox (__chart__, colBox, false);

      // Recursively draw children
      if (__node__.children) {
        for (let i = 0; i < __node__.children.length; i++) {
          let child = __node__.children[i];
          // Build the reverse hierarchy only once
          if (!child.parent) {
            child.parent = __node__;
          }

          // Handle zoomed chart
          if (__node__.zoom_flag == 1) {
            if (child.zoom_flag == 1
            ||  child.zoom_flag == 2) {
              _draw_flames (__chart__, child, __factor__, __depth__ + 1, __offset_h__)
            }
          }

          // Handle global view
          else {
            _draw_flames (__chart__, child, __factor__, __depth__ + 1, __offset_h__)
            __offset_h__ += __get_box_width (child.value, __chart__.size_x, __factor__)
          }
        }
      }
    }
  }



  // ==========================================================================
  function _set_sizes (__chart__) {
    __chart__.size_x = __chart__.canvas.width - PADDING_L - PADDING_R;
    __chart__.size_y = __chart__.canvas.height - PADDING_T - PADDING_B;
  }



  // ==========================================================================
  function _select_font_size (__chart__) {
    let size = 12;
    let is_fit = false;
    let textMeasurement = null;

    while (!is_fit && size > 0) {
      __chart__.ctx.font = size+"px monospace";
      textMeasurement = __chart__.ctx.measureText("a");
      if (textMeasurement.fontBoundingBoxAscent + textMeasurement.fontBoundingBoxDescent < __chart__.flame_height) {
        is_fit = true;
      }
      else {
        size = size - 2;
      }
    }
    __chart__.font_width  = textMeasurement.width;
    __chart__.font_height = textMeasurement.fontBoundingBoxAscent;
  }



  // ==========================================================================
  function show_highlight_flames (event) {
    const target_id = event.target.id;
    const c_id = target_id.replace (/^highlight_/g, "");
    const chart = __charts_by_ids[c_id];

    if (chart) {
      const str = chart.highlight_text_bar.value;
      if (str && str != "") {
        const pattern = new RegExp (str, "g");

        let max_depth = chart.collision_boxes.length;
        for (let depth = 0; depth < max_depth; depth++) {
          let count = chart.collision_boxes[depth].length;
          for (let i = 0; i < count; i++) {
            let colBox = chart.collision_boxes[depth][i];
            if (colBox.label.match (pattern)) {
              colBox.node.sec_color = highligth_color;
            }
            else {
              colBox.node.sec_color = null;
            }
          }
        }
      }
      else {
        let max_depth = chart.collision_boxes.length;
        for (let depth = 0; depth < max_depth; depth++) {
          let count = chart.collision_boxes[depth].length;
          for (let i = 0; i < count; i++) {
            let colBox = chart.collision_boxes[depth][i];
            colBox.node.sec_color = null;
          }
        }
      }
      _this.draw (chart);
    }
  }



  // ==========================================================================
  function __reset_all_rec (__node__) {
    __node__.sec_color = null;
    __node__.zoom_flag = 0;

    // Recursively draw children
    if (__node__.children) {
      for (let i = 0; i < __node__.children.length; i++) {
        let child = __node__.children[i];
        __reset_all_rec (child)
      }
    }
  }



  // ==========================================================================
  function reset_highlight_flames (event) {
    const target_id = event.target.id;
    const c_id = target_id.replace (/^reset_/g, "");
    const chart = __charts_by_ids[c_id];

    if (chart) {
      __reset_all_rec (chart.collision_boxes[0][0].node);
      chart.current_view = "global";
      _this.draw (chart);
    }
  }



  // ==========================================================================
  function swap_view (event) {
    const target_id = event.target.id;
    const c_id = target_id.replace (/^slider_/g, "");
    const chart = __charts_by_ids[c_id];
    if (chart) {
      if (chart.config.view == "top-down") {
        chart.config.view = "bottom-up";
      }
      else {
        chart.config.view = "top-down";
      }
      _this.draw (chart);
    }
  }



  // ==========================================================================
  function show_help () {
    window.alert ("Controls:\n\n"
                 +"Left Click: Zoom on the selected cell.\n"
                 +"Right Click: Reset zoom.\n"
                 +"Highlight: Highlight cells whose label matchs the content of the search bar using the javascript regular expression syntax.\n"
                 +"Reset: Reset both highlights and zoom.");
  }



  // ==========================================================================
  this.create = (__id__, __data__, __config__) => {
    const div = document.getElementById (__id__);
    if (! div) {
      return nil
    }
    if (__charts_by_ids[__id__]) {
      console.log ("warning: flamegraph for "+__id__+" already exists");
      return __charts_by_ids[__id__];
    }

    let chart = {
      config      : __config__,
      data        : __data__,
      div         : div,
      flame_radius:  3,
      current_view: "global",  // Default view at first display
      max_depth   : __get_data_max_depth (__data__[0], 1),  // Data maximal depth
      id          : __chart_count,
      html_id     : "Mflamegraph_"+__chart_count,
      is_over_event_handled: true,
    }
    __chart_count ++;
    __charts_by_ids[__id__]        = chart;
    __canvas_charts[chart.html_id] = chart;

    // Start drawing
    // Insert canvas and tooltip
    let width = ' ';
    if (__config__.width) {
       width = 'width="'+__config__.width+'px" ';
    }
    let height = ' ';
    if (__config__.height) {
       height = 'height="'+__config__.height+'px" ';
    }

    const slider_html           = '<p style="margin-left: 10px;display: inline-block; margin-top: 0px;">Flamegraph Orientation<label class="switch" style="margin-left: 10px;vertical-align: middle;"><input type="checkbox" id="slider_'+__id__+'"><span class="slider round"></span></label></p>'
    const reset_button_html     = '<button id="reset_'+__id__+'" type="button" style="margin-left: 10px; float: right;" title="Reset highligths and zoom" >Reset</button>';
    const highligth_button_html = '<button id="highlight_'+__id__+'" type="button" style="margin-left: 10px; float: right;" title="Highligth cells">Highlight</button>';
    const text_bar_highligth    = '<input  type="text" style="float: right;" placeholder="label"/>';
    const help_button_html      = '<button type="button" style="margin-left: 10px; float: right;" title="help"> ? </button>';

    div.innerHTML = '<div style="margin-top: 5px;">'+slider_html+help_button_html+reset_button_html+highligth_button_html+text_bar_highligth+'</div><canvas id="'+chart.html_id+'" '+width+height+' ></canvas><div class="__Mflamegraph_tooltip" style="display:none;"></div>';


    chart.tooltip = div.children [2];

    // Add events
    let canvas = div.children [1];
    chart.canvas = canvas;
    canvas.onmouseover   = canvas.onmousemove = _handler_over_event;
    canvas.onmouseleave  = _handler_leave_event;
    canvas.onclick       = _handler_onclick;
    canvas.oncontextmenu = _handler_on_rightclick;

    let toggle_view_b = div.children[0].firstChild.children[0].firstChild;
    let help_b  = div.children[0].children[1];
    let reset_b = div.children[0].children[2];
    let show_b  = div.children[0].children[3];
    toggle_view_b.onchange = swap_view;
    help_b.onclick         = show_help;
    reset_b.onclick        = reset_highlight_flames;
    show_b.onclick         = show_highlight_flames;

    chart.highlight_text_bar = div.children[0].children[4];

    return chart;
  }



  // ==========================================================================
  this.draw = (__chart__) => {

    // Handle height --------------------------------------
    // No height found for the container, compute one using a default flame height and the data depth
    if (! __chart__.config.height
    ||  __chart__.config.height == 0) {
      __chart__.flame_height = 14;
      __chart__.canvas.height = (((__chart__.flame_height + 1) * __chart__.max_depth) + PADDING_T + PADDING_B);
    }
    // Else compure flame height based on container height and data depth
    else {
      _set_sizes (__chart__)
      __chart__.flame_height = Math.trunc (__chart__.size_y / __chart__.max_depth) - 1;
    }

    // Handle width ---------------------------------------
    if (! __chart__.config.width
    ||  __chart__.config.width == 0) {
      __chart__.canvas.width = __chart__.div.clientWidth;
    }

    _set_sizes (__chart__)


    //  -> Get horizontal factor: because of some errors, the base element value may not be 100,
    //     so element values must be multiply be a factor in order to fill the container
    __chart__.width_factor = 100 / __chart__.data[0].value;

    // Setup context
    const c = __chart__.div.children [1];
    const rect = c.getBoundingClientRect();
    __chart__.loc_x = rect.left;
    __chart__.loc_y = rect.top;
    __chart__.ctx = c.getContext('2d');
    _select_font_size (__chart__);

    // Draw
    __chart__.collision_boxes = [];
    __chart__.flame_count = 0;
    __chart__.ctx.clearRect(0, 0, __chart__.size_x + PADDING_L + PADDING_R, __chart__.size_y + PADDING_T + PADDING_B); // Clean screen
    _draw_flames (__chart__, __chart__.data[0], __chart__.width_factor, 1, 0);
  }
}
