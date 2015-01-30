/* cpufreq.c - requires GKrellmM 2.0.0 or higher
 *             requires cpufrequtils
 *
 * author: Christoph Winkelmann, <chw@tks6.net>
 *
 * date         version   comment
 * 07/11/2003   0.1       initial release
 * 12/12/2003   0.2       added slider to set cpu frequency
 * 29/12/2003   0.2.1     get cpu frequency from /proc/cpuinfo if
 *                        /proc/sys/cpu/ unavailable
 * 31/12/2003   0.3       added numerical display of cpu frequency
 *                        display slider only
 * 11/01/2004   0.3.1     always get cpu frequency from /proc/cpuinfo
 *                        get khz_max with khz
 *                        save khz_max in configuration
 *                        hand over absolute frequency to cpufreqset
 * 28/01/2004   0.3.3     support for /sys interface
 * 31/01/2004   0.4       make economic and efficient use of available
 *                        interfaces
 * 15/03/2004   0.5       update plugin once a second only
 *                        display cpu temperature optionally
 *                        display cpufreq governor optionally
 *                        toggle governor when clicking on governor name
 *                        optionally set userspace governor when moving slider
 *                        reserve more height for text and slider
 * 16/03/2004   0.5.1     scaling factor for temperature added
 * 18/10/2004   0.5.2     added version number and www-link to about screen
 * 01/11/2005   0.5.3     use new script cpufreqnextgovernor to toggle gov.
 *                        check for ondemand governor when reading frequency
 * 09/11/2005   0.5.4     remove degree sign for encoding incompatibilities
 *                        display slider optionally
 * 24/11/2005   0.5.5     bugfix: configuration was not loaded correctly
 * 02/10/2006   0.6       base on cpufrequtils
 *                        drop temperature support
 *                        SMP support
 * 15/07/2007   0.6.1     bugfix: segmentation fault on wakeup on SMP systems
 *
 *   gcc -Wall -fPIC -Wall `pkg-config --cflags gtk+-2.0` -c cpufreq.c
 *   gcc -shared -lcpufreq -Wl -o cpufreq.so cpufreq.o
 *   gkrellm -p cpufreq.so
 *  
 *  Adapted from the official plugin-demos
 */

#include <gkrellm2/gkrellm.h>
#include <cpufreq.h>

/* version number */
#define  VERSION        "0.6.1"

/* name in the configuration tree */
#define  CONFIG_NAME	"CPUfreq"

/* theme subdirectory for custom images for this plugin  and
   gkrellmrc style name for custom settings
*/
#define  STYLE_NAME	"cpufreq"

#define NCPU_MAX 8

static unsigned int ncpu;
static unsigned long khz_max;
static unsigned long khz[NCPU_MAX];

static const int length = BUFSIZ;
static char governor[NCPU_MAX][256];
static char empty[1];
static char* governor_text[NCPU_MAX];

static GkrellmMonitor* monitor;
static GkrellmPanel* panel;
static GkrellmTicks* pGK;

static GkrellmDecal* text_decal_freq[NCPU_MAX];
static GkrellmDecal* text_decal_gov[NCPU_MAX];

static GkrellmKrell* slider_krell[NCPU_MAX];
static GkrellmKrell* slider_in_motion[NCPU_MAX];
static double slider_value[NCPU_MAX];

static gint gov_enable = 1;
static gint gov_enable_current;
static gint slider_enable = 1;
static gint slider_enable_current;
static gint slider_userspace_enable = 1;
static gint controls_coupled = 0;
static gint style_id;

static void read_governors() {
  unsigned int cpu;
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    struct cpufreq_policy* policy = cpufreq_get_policy(cpu);
    if (policy) {
      strcpy(governor[cpu], policy->governor);
      cpufreq_put_policy(policy);
    } else {
      strcpy(governor[cpu], empty);
    }
  }
}

static void read_khz() {

  if (!gov_enable_current) {
    /* otherwise it has already been read in this update step */
    read_governors();
  }

  /* current freqs */
  unsigned int cpu;
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    khz[cpu] = cpufreq_get_freq_kernel(cpu);

    /* max freq */
    khz_max = khz[cpu] > khz_max ? khz[cpu] : khz_max;
  }

}

static void next_governor(unsigned int cpu) {
  char cmd[length];
  sprintf(cmd, "sudo /usr/sbin/cpufreqnextgovernor %u", cpu);
  system(cmd);
}

static void governor_userspace(unsigned int cpu) {
  char cmd[length];
  sprintf(cmd, "sudo cpufreq-set -c %u -g userspace", cpu);
  system(cmd);
}

static void set_frequency(unsigned int cpu, unsigned long freq) {
  char cmd[length];
  sprintf(cmd, "sudo cpufreq-set -c %u -f %lu", cpu, freq );
  system(cmd);
}

static void update_plugin() {
  if(!pGK->second_tick) {
    return;
  }

  if (gov_enable_current) {
    read_governors();
  }
  read_khz();

  unsigned int cpu;
  if (slider_enable_current) {
    for( cpu=0; cpu<ncpu; ++cpu ) {
      if (!slider_in_motion[cpu]) {
        int krellpos = slider_krell[cpu]->w_scale*khz[cpu]/khz_max;
        gkrellm_update_krell(panel, slider_krell[cpu], krellpos);
      }
    }
  }
  
  char theText[length];
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    sprintf(theText, "%d MHz", (int)((khz[cpu]+500)/1000));
    text_decal_freq[cpu]->x_off = 0;
    gkrellm_draw_decal_text(panel, text_decal_freq[cpu], theText, -1);
    text_decal_gov[cpu]->x_off = 0;
    gkrellm_draw_decal_text(panel, text_decal_gov[cpu], governor_text[cpu], -1);
  }  

  gkrellm_draw_panel_layers(panel);
}

static void update_slider_position(GkrellmKrell *k,
                                   gint x_ev,
                                   unsigned int cpu) {
  if (k) {
    gint x;

    /* Consider only x_ev values that are inside the dynamic range of the
       krell.
    */
    x = x_ev - k->x0;
    if (x < 0)
      x = 0;
    if (x > k->w_scale)
      x = k->w_scale;
    gkrellm_update_krell(panel, k, x);
    gkrellm_draw_panel_layers(panel);

    slider_value[cpu] = (double) x / (double) k->w_scale;  /* ranges 0.0-1.0 */
  }
}

static gint cb_panel_motion(GtkWidget* widget, GdkEventMotion* ev,
                            gpointer data) {
  GdkModifierType state;

  unsigned int cpu;
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    if (slider_in_motion[cpu]) {
      /* Check if button is still pressed, in case missed button_release*/
      state = ev->state;
      if (!(state & GDK_BUTTON1_MASK)) {
        slider_in_motion[cpu] = NULL;
      } else {
        update_slider_position(slider_in_motion[cpu], (gint) ev->x, cpu);
      }
    }
  }
  return TRUE;
}

static gint cb_panel_release(GtkWidget* widget, GdkEventButton* ev,
                             gpointer data) {
  unsigned int cpu;
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    if (slider_in_motion[cpu]) {
      if (slider_userspace_enable) {
        if (controls_coupled) {
          unsigned int icpu;
          for ( icpu=0; icpu<ncpu; ++icpu ) {
            governor_userspace(icpu);
          }
        } else {
          governor_userspace(cpu);
        }
      }
      if (controls_coupled) {
        unsigned int icpu;
        for ( icpu=0; icpu<ncpu; ++icpu ) {
          /* no bug: slider_value is from cpu and not from icpu */
          set_frequency(icpu, (unsigned long)(slider_value[cpu]*khz_max));
        }
      } else {
        set_frequency(cpu, (unsigned long)(slider_value[cpu]*khz_max));
      }
    }
    slider_in_motion[cpu] = NULL;
  }
  return TRUE;
}

static gint cb_panel_press(GtkWidget* widget, GdkEventButton* ev,
                           gpointer data) {
  GkrellmKrell *k;

  if (ev->button == 3) {
    gkrellm_open_config_window(monitor);
    return TRUE;
  }

  unsigned int cpu;
  for ( cpu=0; cpu<ncpu; ++cpu ) {
    /* Check if button was pressed within the space taken
       up by the slider krell.
    */
    slider_in_motion[cpu] = NULL;
    k = slider_krell[cpu];
    if ( slider_enable_current &&
         ev->x >  k->x0 &&
         ev->x <= k->x0 + k->w_scale &&
         ev->y >= k->y0 &&
         ev->y <= k->y0 + k->h_frame ) {
      slider_in_motion[cpu] = k;
    }

    if (slider_in_motion[cpu]) {
      update_slider_position(slider_in_motion[cpu], (gint) ev->x, cpu);
    }

    /* Check if button was pressed within the space taken
       up by the governor text
    */
    if (gov_enable_current && 
        ev->x > text_decal_gov[cpu]->x &&
        ev->x <= text_decal_gov[cpu]->x + text_decal_gov[cpu]->w &&
        ev->y >= text_decal_gov[cpu]->y &&
        ev->y <= text_decal_gov[cpu]->y + text_decal_gov[cpu]->h) {
      /* toggle governor */
      if(controls_coupled) {
        unsigned int icpu;
        for ( icpu=0; icpu<ncpu; ++icpu ) {
          next_governor(icpu);
        }
      } else {
        next_governor(cpu);
      }
    }
  }

  return TRUE;
}


static gint panel_expose_event(GtkWidget* widget, GdkEventExpose* ev) {
  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  panel->pixmap, ev->area.x, ev->area.y, ev->area.x,
                  ev->area.y, ev->area.width, ev->area.height);
  return FALSE;
}

static void create_plugin(GtkWidget* vbox, gint first_create) {
  GkrellmStyle* style;
  GkrellmPiximage* krell_image;
  GkrellmTextstyle* ts;
  gint y = -1;
  unsigned int cpu;

  /* called when GKrellM is re-built at every theme or horizontal
     size change
  */
  if (first_create)
  	panel = gkrellm_panel_new0();

  read_khz();

  for ( cpu=0; cpu<ncpu; ++cpu ) {

    style = gkrellm_meter_style(style_id);
    /* text style */
    ts = gkrellm_meter_textstyle(style_id);

    /* the governor text */
    text_decal_gov[cpu] =
      gkrellm_create_decal_text(panel, "abcdefghijklmnopqrstuvwxyz", ts, style,
        -1,     /* x = -1 places at left margin */
         y,     /* y = -1 places at top margin */
        -1);    /* w = -1 makes decal the panel width minus margins */
    if (gov_enable) {
      governor_text[cpu] = governor[cpu];
      y = text_decal_gov[cpu]->y + text_decal_gov[cpu]->h + 1;
    } else {
      governor_text[cpu] = empty;
    }

    /* the frequency text */
    text_decal_freq[cpu] =
      gkrellm_create_decal_text(panel, "0123456789 MHz", ts, style,
        -1,     /* x = -1 places at left margin */
         y,     /* y = -1 places at top margin */
        -1);    /* w = -1 makes decal the panel width minus margins */

    y = text_decal_freq[cpu]->y + text_decal_freq[cpu]->h + 1;

    /* the slider */
    if (slider_enable) {
      krell_image = gkrellm_krell_slider_piximage();
      gkrellm_set_style_slider_values_default(style,
	y,     /* y offset */
	0, 0); /* Left and right krell margins */
      slider_krell[cpu] = gkrellm_create_krell(panel, krell_image, style);
      y = slider_krell[cpu]->y0 + slider_krell[cpu]->h_frame + 2;

      gkrellm_monotonic_krell_values(slider_krell[cpu], FALSE);
      gkrellm_set_krell_full_scale(slider_krell[cpu],
                                   slider_krell[cpu]->w_scale, 1);
      gkrellm_update_krell(panel, slider_krell[cpu], 0);

      update_slider_position(slider_krell[cpu],
                             slider_krell[cpu]->w_scale*khz[cpu]/khz_max,
                             cpu);
    }
  }

  slider_enable_current = slider_enable;
  gov_enable_current = gov_enable;

  /* configure and create the panel  */
  gkrellm_panel_configure(panel, "", style);
  gkrellm_panel_create(vbox, monitor, panel);

  if (first_create) {
    g_signal_connect(G_OBJECT (panel->drawing_area), "expose_event",
                     G_CALLBACK(panel_expose_event), NULL);
    g_signal_connect(G_OBJECT(panel->drawing_area), "button_press_event",
                     G_CALLBACK(cb_panel_press), NULL );
    g_signal_connect(G_OBJECT(panel->drawing_area), "button_release_event",
                     G_CALLBACK(cb_panel_release), NULL );
    g_signal_connect(G_OBJECT(panel->drawing_area), "motion_notify_event",
                     G_CALLBACK(cb_panel_motion), NULL);
  }
}

/* Configuration */

#define PLUGIN_CONFIG_KEYWORD  STYLE_NAME

static GtkWidget* gov_enable_button;
static GtkWidget* slider_enable_button;
static GtkWidget* slider_userspace_enable_button;
static GtkWidget* controls_coupled_button;

static void save_plugin_config(FILE *f) {
  fprintf(f, "%s khz_max %d\n", PLUGIN_CONFIG_KEYWORD,
          (int)khz_max);
  fprintf(f, "%s gov_enable %d\n", PLUGIN_CONFIG_KEYWORD,
          gov_enable);
  fprintf(f, "%s slider_userspace_enable %d\n", PLUGIN_CONFIG_KEYWORD,
          slider_userspace_enable);
  fprintf(f, "%s slider_enable %d\n", PLUGIN_CONFIG_KEYWORD,
          slider_enable);
  fprintf(f, "%s controls_coupled %d\n", PLUGIN_CONFIG_KEYWORD,
          controls_coupled);
}

static void load_plugin_config(gchar *arg) {
  gchar	config[64], item[256];
  gint n;
  int khz_max_config;

  n = sscanf(arg, "%s %[^\n]", config, item);
  if (n == 2) {
    if (strcmp(config, "khz_max") == 0) {
      sscanf(item, "%d\n", &khz_max_config);
      if (khz_max_config > khz_max) {
        khz_max = khz_max_config;
      }
    } else if (strcmp(config, "slider_enable") == 0) {
      sscanf(item, "%d", &slider_enable);
      slider_enable_current = slider_enable;
    } else if (strcmp(config, "slider_userspace_enable") == 0) {
      sscanf(item, "%d", &slider_userspace_enable);
    } else if (strcmp(config, "gov_enable") == 0) {
      sscanf(item, "%d", &gov_enable);
      gov_enable_current = gov_enable;
      unsigned int cpu;
      for ( cpu=0; cpu<ncpu; ++cpu ) {
        governor_text[cpu] = gov_enable ? governor[cpu] : empty;
      }
    } else if (strcmp(config, "controls_coupled") == 0) {
      sscanf(item, "%d", &controls_coupled);
    }
  }
}

static void apply_plugin_config(void) {
  gov_enable =
    GTK_TOGGLE_BUTTON(gov_enable_button)->active;
  slider_enable =
    GTK_TOGGLE_BUTTON(slider_enable_button)->active;
  slider_userspace_enable =
    GTK_TOGGLE_BUTTON(slider_userspace_enable_button)->active;
  controls_coupled =
    GTK_TOGGLE_BUTTON(controls_coupled_button)->active;
}

static gchar  *plugin_info_text[] = {
  "<h>CPU frequency plugin\n",
  "gkrellm2-cpufreq ",VERSION,", ",
  "<ul> http://chw.tks6.net/gkrellm2-cpufreq/\n",
  "by Christoph Winkelmann, ",
  "<ul> chw@tks6.net\n\n",
  "Enabling and disabling governor or slider display only takes effect\n",
  "after change of theme, change of width, or restart.\n\n",
  "The maximal frequency found during this run is saved for the next run\n",
  "when this page is viewed.\n\n",
  "Any suggestions are welcome!",
  /* "\n\tPut any documentation here to explain your plugin.\n",
     "\tText can be ",
     "<b>bold",
     " or ",
     "<i>italic",
     " or ",
     "<ul>underlined.\n",
  */
};

static void create_plugin_tab(GtkWidget *tab_vbox) {
  GtkWidget* tabs;
  GtkWidget* vbox;
  GtkWidget* vbox1;
  GtkWidget* text;
  gint i;

  tabs = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

  /* -- Options tab -- */
  vbox = gkrellm_gtk_framed_notebook_page(tabs, "Options");

  /* -- Governor options -- */
  vbox1 = gkrellm_gtk_framed_vbox(vbox, "CPUfreq governor", 4, FALSE, 0, 2);
  gkrellm_gtk_check_button(vbox1, &gov_enable_button,
                           gov_enable, FALSE, 0,
                           "Show governor (see Info tab)");
  gkrellm_gtk_check_button(vbox1, &slider_userspace_enable_button,
                           slider_userspace_enable, FALSE, 0,
                           "Set userspace governor when moving slider");

  /* -- Slider options -- */
  vbox1 = gkrellm_gtk_framed_vbox(vbox, "Slider", 4, FALSE, 0, 2);
  gkrellm_gtk_check_button(vbox1, &slider_enable_button,
                           slider_enable, FALSE, 0,
                           "Show slider (see Info tab)");

  /* -- SMP options -- */
  vbox1 = gkrellm_gtk_framed_vbox(vbox, "SMP", 4, FALSE, 0, 2);
  gkrellm_gtk_check_button(vbox1, &controls_coupled_button,
                           controls_coupled, FALSE, 0,
                           "Couple controls of multiple CPUs");

  /* -- Info tab -- */
  vbox = gkrellm_gtk_framed_notebook_page(tabs, "Info");
  text = gkrellm_gtk_scrolled_text_view(vbox, NULL, GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
  for (i = 0; i < sizeof(plugin_info_text)/sizeof(gchar *); ++i)
  	gkrellm_gtk_text_view_append(text, plugin_info_text[i]);
}

/* The monitor structure tells GKrellM how to call the plugin routines.
*/
static GkrellmMonitor  plugin_mon = {
  CONFIG_NAME,           /* Title for config clist                       */
  0,                     /* Id,  0 if a plugin                           */
  create_plugin,         /* The create function                          */
  update_plugin,         /* The update function                          */
  create_plugin_tab,     /* The config tab create function               */
  apply_plugin_config,   /* Apply the config function                    */

  save_plugin_config,    /* Save user config                             */
  load_plugin_config,    /* Load user config                             */
  PLUGIN_CONFIG_KEYWORD, /* config keyword                               */

  NULL,                  /* Undefined 2	                                 */
  NULL,                  /* Undefined 1	                                 */
  NULL,                  /* private                                      */

  MON_CPU,               /* Insert plugin before this monitor            */

  NULL,                  /* Handle if a plugin, filled in by GKrellM     */
  NULL                   /* path if a plugin, filled in by GKrellM       */
};


/* All GKrellM plugins must have one global routine named init_plugin()
   which returns a pointer to a filled in monitor structure.
*/

GkrellmMonitor* gkrellm_init_plugin(void)

{
  /* If this next call is made, the background and krell images for this
     plugin can be custom themed by putting bg_meter.png or krell.png in the
     subdirectory STYLE_NAME of the theme directory.  Text colors (and
     other things) can also be specified for the plugin with gkrellmrc
     lines like:  StyleMeter  STYLE_NAME.textcolor orange black shadow
     If no custom theming has been done, then all above calls using
     style_id will be equivalent to style_id = DEFAULT_STYLE_ID.
  */
  pGK = gkrellm_ticks();
  style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
  monitor = &plugin_mon;

  /* determine number of cpus */
  for( ncpu = 0; cpufreq_cpu_exists(ncpu)==0; ++ncpu )
    ;
  ncpu = ncpu > NCPU_MAX ? NCPU_MAX : ncpu;

  /* determine maximal frequency */
  unsigned int cpu;
  for ( cpu = 0; cpu<ncpu; ++cpu) {
    unsigned long khz_min = 0;
    unsigned long khz_max_i = 0;
    cpufreq_get_hardware_limits( cpu, &khz_min, &khz_max_i );
    khz_max = khz_max < khz_max_i ? khz_max_i : khz_max;
  }

  read_khz();

  strcpy(empty, "");
  if (gov_enable_current) {
    read_governors();
  } else {
    for ( cpu=0; cpu<ncpu; ++cpu ) {
      strcpy(governor[cpu], "");
    }
  }
  return &plugin_mon;
}
