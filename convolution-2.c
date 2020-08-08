// ========================================
// KNIHOVNY
// ========================================

#include <libgimp/gimp.h> // pro praci s plug-iny
#include <libgimp/gimpui.h> // pro praci s GUI
#include <stdlib.h> // pro praci s absolutnimi hodnotami

// ========================================
// FUNKCNI PROTOTYPY
// ========================================

static void query	   	   		(void);
static void run  	   	   		(const gchar       *name,
							 	 gint               nparams,
							 	 const GimpParam   *param,
							 	 gint              *nreturn_vals,
							 	 GimpParam        **return_vals);
static void init_mem  			(guchar          ***row,
					  		 	 guchar           **outrow,
					  		 	 gint               num_of_bytes);
static void convolution_2		(GimpDrawable 	   *drawable,
							 	 GimpPreview       *preview);
static void s_changes			(GtkWidget         *widget,
								 GtkWidget        **gui_objects);
static void gb_changes			(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void ed_1_changes		(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void e_changes			(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void matrix_changes		(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void radius_changes		(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void customize_changes	(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static void preview_changes		(GtkWidget 		   *widget,
								 GtkWidget        **gui_objects);
static gboolean show_dialog 	(GimpDrawable 	   *drawable);

// ========================================
// PLUG-IN
// ========================================

/* Struktura pro zachovani uzivatelem nastavenych hodnot. */
typedef struct
{
	/* Radius a konvolucni matice. */
	gint radius;
	int matrix[49];
} gui_data;

/* Pocatecni nastaveni teto struktury. */
static gui_data gui_values =
{
	1,
	{
		-1, -1, -1,
		-1,  9, -1,
		-1, -1, -1
	}
};

/*
	PLUG_IN_INFO je globalni promenna, ktera obsahuje
	ukazatele na ctyri zakladni funkce plug-inu.
*/
GimpPlugInInfo PLUG_IN_INFO =
{
	NULL, // volana, kdyz se GIMP zapina
	NULL, // volana, kdyz se GIMP vypina
	
	/*
		Volana, aby informovala GIMP o tom, co plug-in
		dela, a registrovala jej v databazi procedur
		(dale jen „PDB“). 
	*/
	query,
	
	/*
		Volana, aby spustila plug-in, ktery je jiz
		zaregistrovany v PDB.
	*/
	run
};

/*
	MAIN() je makro, ktere zavola funkci gimp_main().
	Ta zaridi spoustu dulezitych veci, jako napr.
	zakladni komunikaci mezi plug-inem a GIMPem atd.
*/
MAIN()

// ========================================

/* Funkce provadejici registraci plug-inu. */
static void query (void)
{
	/* Definice vstupnich parametru. */
	static GimpParamDef args[] =
	{
		/* Typ, jmeno a popis parametru. */
		{GIMP_PDB_INT32,    "run-mode", "Run mode"},
		{GIMP_PDB_IMAGE,    "image",    "Input image"},
		{GIMP_PDB_DRAWABLE, "drawable", "Input drawable"}
	};

	/* Zaregistruje plug-in do PDB. */
	gimp_install_procedure (
		"plug-in-convolution-2", // nazev
		"Convolution 2", // kratsi popisek
		"Modify an image with sharpening, blurring and other convolution filters.", // delsi popisek
		"Karel Vrabec", // autor
		"© Karel Vrabec", // copyright
		"2019", // rok
		"_Convolution 2...", // text polozky v menu
		"RGB*, GRAY*", // podporovane formaty
		GIMP_PLUGIN, // typ
		G_N_ELEMENTS(args), // pocet vstupnich parametru
		0, // pocet vystupnich parametru
		args, // pole vstupnich parametru
		NULL // pole vystupnich parametru
	);

	/* Zaregistruje plug-in do menu. */
	gimp_plugin_menu_register (
		"plug-in-convolution-2", // nazev
		"<Image>/Filters/Misc" // cesta
	);
} // QUERY

// ========================================

/* Funkce provadejici hlavni praci plug-inu. */
static void run (const gchar      *name, // nazev plug-inu
				 gint              nparams, // pocet vstupnich parametru
				 const GimpParam  *param, // vstupni parametry
				 gint             *nreturn_vals, // pocet vystupnich parametru
				 GimpParam       **return_vals) // vystupni parametry
{
	static GimpParam  values[1]; // pole navratovych hodnot
	GimpPDBStatusType status = GIMP_PDB_SUCCESS; // status
	GimpRunMode       run_mode = param[0].data.d_int32; // mod
	GimpDrawable     *drawable = gimp_drawable_get(param[2].data.d_drawable); // obrazkova data
	
	/* Nastaveni navratove hodnoty. */
	*nreturn_vals = 1;
	*return_vals = values;

	values[0].type = GIMP_PDB_STATUS;
	values[0].data.d_status = status;

	/* Primitivni kontrola rozmeru obrazku. */
	if (drawable->width > 4096 || drawable->height > 4096)
	{
		g_message("Cannot work with dimensions larger than 4K!");
		return;
	} // if

	/* Primitivni kontrola velikosti obrazku. */
	if ((drawable->width)*(drawable->height)*(gimp_drawable_bpp(drawable->drawable_id)) > 50000000) // 50 MB
	{
		g_message("The image size is too large!");
		return;		
	} // if
	
	/* Osetreni modu. */
	switch (run_mode)
	{
		/* Spusten z GIMPu. */
		case GIMP_RUN_INTERACTIVE:

			/* Nastavi posledni pouzite hodnoty. */
			gimp_get_data("plug-in-convolution-2", &gui_values);

			/* Zobrazi dialog. */
			if (!show_dialog(drawable))
				return;

			/* Ulozi posledni pouzite hodnoty. */
			gimp_set_data("plug-in-convolution-2", &gui_values, sizeof(gui_data));

			break;

		/* Spusten ze skriptu. */
		case GIMP_RUN_NONINTERACTIVE:

			/* Kontrola poctu parametru. */
			if (nparams != 4)
				status = GIMP_PDB_CALLING_ERROR;

			/* Nastavi radius. */
			if (status == GIMP_PDB_SUCCESS)
				gui_values.radius = param[3].data.d_int32;

			break;

		/* Spusten jako opakovani posledni upravy. */
		case GIMP_RUN_WITH_LAST_VALS:

			/* Nastavi posledni pouzite hodnoty. */
			gimp_get_data("plug-in-convolution-2", &gui_values);

			break;

		default:
			break;
	} // switch

	/* Uprava obrazku pomoci konvoluce. */
	convolution_2(drawable, NULL);

	/* Aktualizace obrazku a uvolneni pameti. */
	gimp_displays_flush();
	gimp_drawable_detach(drawable);
} // RUN

// ========================================
// ALGORITMUS KONVOLUCE
// ========================================

/* Funkce alokujici pamet. */
static void init_mem (guchar ***row,
					  guchar  **outrow,
					  gint      num_of_bytes)
{
	*row = g_new(guchar *, 2*gui_values.radius+1); // alokuje misto pro ukazatele na radky pixelu
	for (int x = 0; x < (2*gui_values.radius+1); x++) // tyto ukazatele postupne projede
		(*row)[x] = g_new(guchar, num_of_bytes); // alokuje radky pixelu
	*outrow = g_new(guchar, num_of_bytes); // alokuje radek pro nove vytvorene pixely
} // INIT_MEM

// ========================================

/* Funkce provadejici konvoluci. */
static void convolution_2 (GimpDrawable *drawable,
						   GimpPreview  *preview)
{
	gint           i, j, k, l, m, x1, y1, x2, y2, width, height, channels, normalizer, sum, conv_index;
	GimpPixelRgn   rgn_in, rgn_out;
	guchar       **row;
	guchar        *outrow;

	if (preview) // probehne pro nahled
	{
		gimp_preview_get_position(preview, &x1, &y1);
		gimp_preview_get_size(preview, &width, &height);
		x2 = x1+width;
		y2 = y1+height;
	} // if
	else // probehne pro cely obrazek
	{
		gimp_drawable_mask_bounds(drawable->drawable_id, &x1, &y1, &x2, &y2);
		width = x2-x1;
		height = y2-y1;
	} // else

	/* Pouziva cache pro zrychleni cteni a zapisu (ponevadz zpracovavame radek po radku). */
	gimp_tile_cache_ntiles(2*(width/gimp_tile_width()+1));

	/* Vrati pocet kanalu na pixel. */
	channels = gimp_drawable_bpp(drawable->drawable_id);

	/* Inicializuje oblast pixelu pro cteni dat. */
	gimp_pixel_rgn_init(&rgn_in, drawable, x1, y1, width, height, FALSE, FALSE);

	/* Inicializuje oblast pixelu pro zapis dat. */
	gimp_pixel_rgn_init(&rgn_out, drawable, x1, y1, width, height, preview == NULL, TRUE);

	/* Inicializace pameti. */
	init_mem(&row, &outrow, channels*width);

	/* Inicializace ukazatele prubehu. */
	if (!preview)
		gimp_progress_init("Working on it...");

	/* Vypocet normalizacni hodnoty. */
	normalizer = 0;
	for (i = 0; i < (2*gui_values.radius+1)*(2*gui_values.radius+1); i++)
		normalizer += gui_values.matrix[i]; // secte vsechny hodnoty v konvolucni matici
	if (!normalizer) normalizer = 1; // pro pripad, ze normalizacni hodnota bude 0

	/* Zpracovava obrazek po radach (okraje resi zrcadlove). */
	for (i = 0; i < height; i++)
	{
		/* Nacita pixely do alokovanych rad. */
		for (j = 0; j < (2*gui_values.radius+1); j++)
		{
			if ((i+y1)-gui_values.radius+j < y1) // v pripade prvni rady
				gimp_pixel_rgn_get_row(&rgn_in, row[j], x1, abs((i+y1)-gui_values.radius+j) - 1, width);
			else if ((i+y1)-gui_values.radius+j > y2-1) // v pripade posledni rady
				gimp_pixel_rgn_get_row(&rgn_in, row[j], x1, y2 - (((i+y1)-gui_values.radius+j) - (y2-1)), width);
			else // v pripade vsech ostatnich rad
				gimp_pixel_rgn_get_row(&rgn_in, row[j], x1, (i+y1)-gui_values.radius+j, width);
		} // for

		/* Zpracovava jednotlive pixely (okraje resi zrcadlove). */
		for (j = 0; j < width; j++)
		{
			/* Pro kazdy kanal spocita prumer ze vsech 9 pixelu. */
			for (k = 0; k < channels; k++)
			{
				sum = conv_index = 0;
				
				/* Zpracovava okolni pixely. */
				for (l = 0; l < (2*gui_values.radius+1); l++)
					for (m = j - gui_values.radius; m <= j + gui_values.radius; m++)
					{
						if (m < 0) // v pripade prvniho sloupce
							sum += gui_values.matrix[conv_index++] * row[l][channels * (abs(m)-1) + k];
						else if (m > (width-1)) // v pripade posledniho sloupce
							sum += gui_values.matrix[conv_index++] * row[l][channels * (width - (m - (width-1))) + k];
						else // v pripade vsech ostatnich sloupcu
							sum += gui_values.matrix[conv_index++] * row[l][channels * m + k];
					} // for
				
				/* Ulozeni vysledne hodnoty a osetreni preteceni. */
				outrow[channels*j+k] = MIN(MAX(sum/normalizer, 0), 255);
			} // for
		} // for

		/* Ulozi radu nove propocitanych pixelu. */
		gimp_pixel_rgn_set_row(&rgn_out, outrow, x1, i+y1, width);

		/* Aktualizace ukazatele prubehu. */
		if (!preview && i % 10 == 0)
			gimp_progress_update((gdouble) i / (gdouble) y2);
	} // for

	/* Uvolneni pameti. */
	for (i = 0; i < (2*gui_values.radius+1); i++)
		g_free(row[i]);
	g_free(row);
	g_free(outrow);

	/* Aktualizace obrazku. */
	if (preview) // probehne pro nahled
		gimp_drawable_preview_draw_region(GIMP_DRAWABLE_PREVIEW(preview), &rgn_out);
	else // probehne pro cely obrazek
	{
		gimp_drawable_flush(drawable); // odesle vsechny zmeny
		gimp_drawable_merge_shadow(drawable->drawable_id, TRUE); // aplikuje nove pixely na stare
		gimp_drawable_update(drawable->drawable_id, x1, y1, width, height); // aktualizuje obrazek
	} // else
} // CONVOLUTION_2

// ========================================
// POSLUCHACE UDALOSTI
// ========================================

/* Posluchac tlacitka pro predvolbu Sharpen. */
static void s_changes(GtkWidget  *widget,
					  GtkWidget **gui_objects)
{
	int s[] = {-1, -1, -1, -1, 9, -1, -1, -1, -1};
	for (int i = 0; i < 9; i++)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */), s[i]);
	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */)); // aktualizuje nahled
} // S CHANGES

// ========================================

/* Posluchac tlacitka pro predvolbu Gaussian Blur. */
static void gb_changes(GtkWidget  *widget,
					   GtkWidget **gui_objects)
{
	int gb[] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
	for (int i = 0; i < 9; i++)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */), gb[i]);
	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */));
} // GB CHANGES

// ========================================

/* Posluchac tlacitka pro predvolbu Edge Detection 1. */
static void ed_1_changes(GtkWidget  *widget,
						 GtkWidget **gui_objects)
{
	int ed_1[] = {1, 0, -1, 0, 0, 0, -1, 0, 1};
	for (int i = 0; i < 9; i++)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */), ed_1[i]);
	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */));
} // ED 1 CHANGES

// ========================================

/* Posluchac tlacitka pro predvolbu Emboss. */
static void e_changes(GtkWidget  *widget,
					  GtkWidget **gui_objects)
{
	int e[] = {-2, -1, 0, -1, 1, 1, 0, 1, 2};
	for (int i = 0; i < 9; i++)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */), e[i]);
	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */));
} // E CHANGES

// ========================================

/* Posluchac pro zmenu konvolucni matice. */
static void matrix_changes(GtkWidget  *widget,
						   GtkWidget **gui_objects)
{
	for (int i = 0; i < 9; i++)
		gui_values.matrix[i] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */));
	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */));
} // MATRIX CHANGES

// ========================================

/* Posluchac pro zmenu radiusu. */
static void radius_changes(GtkWidget  *widget,
						   GtkWidget **gui_objects)
{
	gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(gui_objects[11]/* filter_combobox */));
	if (value == 0 || value == 2)
	{
		gtk_widget_show(gui_objects[12]/* radius_scale */);
		gtk_adjustment_set_upper(GTK_ADJUSTMENT(gui_objects[13]/* radius_scale_core */), 3); // nastavi horni hranici
	} // if
	else if (value == 1)
	{	
		gtk_widget_show(gui_objects[12]/* radius_scale */);
		gtk_adjustment_set_upper(GTK_ADJUSTMENT(gui_objects[13]/* radius_scale_core */), 2);
	} // else if
	else
		gtk_widget_hide(gui_objects[12]/* radius_scale */);

	gtk_adjustment_set_value(GTK_ADJUSTMENT(gui_objects[13]/* radius_scale_core */), 1); // nastavi aktualni hodnotu
} // RADIUS CHANGES

// ========================================

/* Posluchac pro vlastni filtr. */
static void customize_changes(GtkWidget  *widget,
							  GtkWidget **gui_objects)
{
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == 7)
	{
		gtk_widget_show(gui_objects[14]/* matrix */);
		gtk_widget_show(gui_objects[15]/* presets */);
	} // if
	else
	{
		gtk_widget_hide(gui_objects[14]/* matrix */);
		gtk_widget_hide(gui_objects[15]/* presets */);
		gtk_window_resize(GTK_WINDOW(gui_objects[9]/* dialog */), 1, 1); // zminimalizuje okno
	} // else
} // CUSTOMIZE CHANGES

// ========================================

/* Zmena stavu nahledu. */
static void preview_changes(GtkWidget  *widget,
							GtkWidget **gui_objects)
{
	gint filter, radius, size, i;
	filter = gtk_combo_box_get_active(GTK_COMBO_BOX(gui_objects[11]/* filter_combobox */));
	radius = gtk_adjustment_get_value(GTK_ADJUSTMENT(gui_objects[13]/* radius_scale_core */));
	size = (2*radius+1)*(2*radius+1); // pocet prvku matice

	/* Preda konvolucni matici algoritmu. */
	if (filter == 0) /* Sharpen */
	{
		gui_values.radius = radius;
		for (i = 0; i < size; i++)
			if (i == size/2) gui_values.matrix[i] = size;
			else gui_values.matrix[i] = -1;
	} // if
	else if (filter == 1) /* Gaussian Blur */
	{
		if (radius == 1)
		{
			static gui_data tmp =
			{
				1,
				{1, 2, 1,
				 2, 4, 2,
				 1, 2, 1}
			};
			gui_values = tmp;
		} // if
		else if (radius == 2)
		{
			static gui_data tmp =
			{
				2,
				{1,  4,  6,  4,  1,
				 4, 16, 24, 16,  4,
				 6, 24, 36, 24,  6,
				 4, 16, 24, 16,  4,
				 1,  4,  6,  4,  1}
			};
			gui_values = tmp;
		} // else if
	} // else if
	else if (filter == 2) /* Box Blur */
	{
		gui_values.radius = radius;
		for (i = 0; i < size; i++)
			gui_values.matrix[i] = 1;
	} // else if
	else if (filter == 3) /* Edge Detection 1. */
	{
		static gui_data tmp =
		{
			1,
			{1,  0, -1,
			 0,  0,  0,
			-1,  0,  1}
		};
		gui_values = tmp;
	} // else if
	else if (filter == 4) /* Edge Detection 2. */
	{
		static gui_data tmp =
		{
			1,
			{0,  1,  0,
			 1, -4,  1,
			 0,  1,  0}
		};
		gui_values = tmp;
	} // else if		
	else if (filter == 5) /* Edge Detection 3. */
	{
		static gui_data tmp =
		{
			1,
			{-1, -1, -1,
			 -1,  8, -1,
			 -1, -1, -1}
		};
		gui_values = tmp;
	} // else if		
	else if (filter == 6) /* Emboss. */
	{
		static gui_data tmp =
		{
			1,
			{-2, -1,  0,
			 -1,  1,  1,
			  0,  1,  2}
		};
		gui_values = tmp;
	} // else if	
	else if (filter == 7) /* Customize. */
	{
		gui_values.radius = 1;
		for (i = 0; i < 9; i++)
			gui_values.matrix[i] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui_objects[i]/* spinbutton_x_y */));
	} // else if

	gimp_preview_invalidate(GIMP_PREVIEW(gui_objects[10]/* preview */));
} // PREVIEW CHANGES

// ========================================
// GUI
// ========================================

/* Funkce zobrazujici hlavni dialog. */
static gboolean show_dialog (GimpDrawable *drawable)
{
	/* Inicializuje GUI. */
	gimp_ui_init("plug-in-convolution-2", FALSE);
	
	/* Vytvori hlavni dialog. */
	GtkWidget *dialog = gimp_dialog_new(
		"Convolution 2", // nazev
		"plug-in-convolution-2", // plug-in
		NULL, // rodicovske okno
		0, // flagy
		gimp_standard_help_func, // pomocna funkce volana po stisknuti F1
		"plug-in-convolution-2", // argument pomocne funkce
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, // tlacitko pro ukonceni
		GTK_STOCK_OK, GTK_RESPONSE_OK, // tlacitko pro pokracovani
		NULL // ostatni
	);

	/* Pripoji hlavni dialog k hlavnimu oknu GIMPu. */
	gimp_window_set_transient(GTK_WINDOW(dialog));

	// ========================================

	/*
		Kazdy dialog ma atributy "vbox" (hlavni
		kontejner, ve kterem jsou ulozeny veskere
		widgety) a "action_area" (oblast, ktera
		obsahuje vsechna dulezita tlacitka). Tyto
		atributy nelze nijak menit.	Lze je pouze 
		vyuzit ke tvorbe dalsich casti dialogu.
	*/

	// ========================================

	/* Frame pro nastaveni. */
	GtkWidget *settings = gtk_frame_new(NULL); // vytvori frame
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), settings, FALSE, FALSE, 5); // prida frame do vboxu
	gtk_container_set_border_width(GTK_CONTAINER(settings), 5); // prida ramecek
	gtk_widget_show(settings); // zobrazi frame

	/* Popisek pro nastaveni. */
	GtkWidget *settings_label = gtk_label_new("Settings");
	gtk_frame_set_label_widget(GTK_FRAME(settings), settings_label); // nastavi popisek na frame
	gtk_widget_show(settings_label);

	/* Zarovnani pro nastaveni. */
	GtkWidget *settings_align = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_container_add(GTK_CONTAINER(settings), settings_align); // prida zarovnani do framu
	gtk_alignment_set_padding(GTK_ALIGNMENT(settings_align), 5, 5, 5, 5); // nastavi odsazeni od vsech stran
	gtk_widget_show(settings_align);

	/* Box pro nastaveni. */
	GtkWidget *settings_box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(settings_align), settings_box);
	gtk_widget_show(settings_box);

	/* Nahled. */
	GtkWidget *preview = gimp_drawable_preview_new(drawable, 0);
	gimp_preview_set_bounds(
		GIMP_PREVIEW(preview),
		(drawable->width/2)-125,
		(drawable->height/2)-125,
		(drawable->width/2)+125,
		(drawable->height/2)+125
	); // nastavi velikost vyrezu
	gtk_box_pack_start(GTK_BOX(settings_box), preview, FALSE, FALSE, 5);
	gtk_widget_show(preview);

	// ========================================

	/* Box pro filtr. */
	GtkWidget *filter_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(settings_box), filter_box, FALSE, FALSE, 5);
	gtk_widget_show(filter_box);

	/* Popisek pro vyber filtru. */
	GtkWidget *filter_combobox_label = gtk_label_new_with_mnemonic("_Filter:"); // vytvori popisek s podtrzenym prvnim pismenem
	gtk_box_pack_start(GTK_BOX(filter_box), filter_combobox_label, FALSE, FALSE, 5);
	gtk_widget_show(filter_combobox_label);

	/* Vyber filtru. */
	GtkWidget *filter_combobox = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Sharpen");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Gaussian Blur");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Box Blur");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Edge Detection 1");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Edge Detection 2");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Edge Detection 3");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Emboss");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combobox), "Customize...");
	gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combobox), 0); // nastavi defaultni hodnotu
	gtk_box_pack_start(GTK_BOX(filter_box), filter_combobox, FALSE, FALSE, 5);
	gtk_widget_show(filter_combobox);

	// ========================================

	/* Box pro radius. */
	GtkWidget *radius_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(settings_box), radius_box, FALSE, FALSE, 5);
	gtk_widget_show(radius_box);

	/* Popisek pro stupnici radiusu. */
	GtkWidget *radius_scale_label = gtk_label_new_with_mnemonic("_Radius:");
	gtk_box_pack_start(GTK_BOX(radius_box), radius_scale_label, FALSE, FALSE, 5);
	gtk_widget_show(radius_scale_label);

	/* Stupnice radiusu. */
	GtkObject *radius_scale_core = gtk_adjustment_new(1, 1, 3, 1, 1, 0); // defaultni hodnota, minimum, maximum, krok a dalsi
	GtkWidget *radius_scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(radius_scale_core));
	gtk_scale_set_digits(GTK_SCALE(radius_scale), 0); // nastavi cela cisla
	gtk_widget_set_size_request(radius_scale, 100, -1); // nastavi velikost, -1 je oznaceni pro puvodni hodnotu
	gtk_box_pack_start(GTK_BOX(radius_box), radius_scale, FALSE, FALSE, 5);
	gtk_widget_show(radius_scale);
	
	// ========================================

	/* Frame pro matici. */
	GtkWidget *matrix = gtk_frame_new (NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), matrix, FALSE, FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(matrix), 5);

	/* Popisek pro matici. */
	GtkWidget *matrix_label = gtk_label_new("Matrix");
	gtk_frame_set_label_widget(GTK_FRAME(matrix), matrix_label);
	gtk_widget_show(matrix_label);

	/* Zarovnani pro matici. */
	GtkWidget *matrix_align = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_container_add(GTK_CONTAINER(matrix), matrix_align);
	gtk_alignment_set_padding(GTK_ALIGNMENT(matrix_align), 5, 5, 5, 5);
	gtk_widget_show(matrix_align);

	/* Box pro matici. */
	GtkWidget *matrix_box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(matrix_align), matrix_box);
	gtk_widget_show(matrix_box);

	// ========================================

	/* Prvni radek matice. */
	GtkWidget *matrix_box_1 = gtk_hbox_new(FALSE, 0); // horizontalni box pro prvky
	gtk_box_pack_start(GTK_BOX(matrix_box), matrix_box_1, FALSE, FALSE, 5);
	gtk_widget_show(matrix_box_1);

	/* Prvni prvek radku. */
	GtkObject *spinbutton_0_0_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_0_0 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_0_0_core), 1, 0); // skala, krok, desetinna mista
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_0_0), TRUE); // povoli zadavat pouze cisla
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_0_0), gui_values.matrix[0]); // v pripade radiusu 1 nastavi predesle hodnoty
	gtk_box_pack_start(GTK_BOX(matrix_box_1), spinbutton_0_0, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_0_0);

	/* Druhy prvek radku. */
	GtkObject *spinbutton_0_1_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_0_1 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_0_1_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_0_1), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_0_1), gui_values.matrix[1]);
	gtk_box_pack_start(GTK_BOX(matrix_box_1), spinbutton_0_1, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_0_1);

	/* Treti prvek radku. */
	GtkObject *spinbutton_0_2_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_0_2 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_0_2_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_0_2), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_0_2), gui_values.matrix[2]);
	gtk_box_pack_start(GTK_BOX(matrix_box_1), spinbutton_0_2, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_0_2);

	// ========================================

	/* Druhy radek matice. */
	GtkWidget *matrix_box_2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(matrix_box), matrix_box_2, FALSE, FALSE, 5);
	gtk_widget_show(matrix_box_2);

	/* Prvni prvek radku. */
	GtkObject *spinbutton_1_0_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_1_0 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_1_0_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_1_0), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_1_0), gui_values.matrix[3]);
	gtk_box_pack_start(GTK_BOX(matrix_box_2), spinbutton_1_0, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_1_0);

	/* Druhy prvek radku. */
	GtkObject *spinbutton_1_1_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_1_1 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_1_1_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_1_1), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_1_1), gui_values.matrix[4]);
	gtk_box_pack_start(GTK_BOX(matrix_box_2), spinbutton_1_1, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_1_1);

	/* Treti prvek radku. */
	GtkObject *spinbutton_1_2_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_1_2 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_1_2_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_1_2), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_1_2), gui_values.matrix[5]);
	gtk_box_pack_start(GTK_BOX(matrix_box_2), spinbutton_1_2, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_1_2);

	// ========================================

	/* Treti radek matice. */
	GtkWidget *matrix_box_3 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(matrix_box), matrix_box_3, FALSE, FALSE, 5);
	gtk_widget_show(matrix_box_3);

	/* Prvni prvek radku. */
	GtkObject *spinbutton_2_0_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_2_0 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_2_0_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_2_0), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_2_0), gui_values.matrix[6]);
	gtk_box_pack_start(GTK_BOX(matrix_box_3), spinbutton_2_0, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_2_0);

	/* Druhy prvek radku. */
	GtkObject *spinbutton_2_1_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_2_1 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_2_1_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_2_1), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_2_1), gui_values.matrix[7]);
	gtk_box_pack_start(GTK_BOX(matrix_box_3), spinbutton_2_1, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_2_1);

	/* Treti prvek radku. */
	GtkObject *spinbutton_2_2_core = gtk_adjustment_new(0, -100, 100, 1, 0, 0);
	GtkWidget *spinbutton_2_2 = gtk_spin_button_new(GTK_ADJUSTMENT(spinbutton_2_2_core), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbutton_2_2), TRUE);
	if (gui_values.radius == 1) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton_2_2), gui_values.matrix[8]);
	gtk_box_pack_start(GTK_BOX(matrix_box_3), spinbutton_2_2, FALSE, FALSE, 5);
	gtk_widget_show(spinbutton_2_2);

	// ========================================

	/* Frame pro predvolby. */
	GtkWidget *presets = gtk_frame_new (NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), presets, FALSE, FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(presets), 5);

	/* Popisek pro predvolby. */
	GtkWidget *presets_label = gtk_label_new("Presets");
	gtk_frame_set_label_widget(GTK_FRAME(presets), presets_label);
	gtk_widget_show(presets_label);

	/* Zarovnani pro predvolby. */
	GtkWidget *presets_align = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_container_add(GTK_CONTAINER(presets), presets_align);
	gtk_alignment_set_padding(GTK_ALIGNMENT(presets_align), 5, 5, 5, 5);
	gtk_widget_show(presets_align);

	/* Box pro predvolby. */
	GtkWidget *presets_box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(presets_align), presets_box);
	gtk_widget_show(presets_box);

	// ========================================

	/* Tlacitko pro Sharpen. */
	GtkWidget *sharpen_button = gtk_button_new_with_label("Sharpen");
	gtk_box_pack_start(GTK_BOX(presets_box), sharpen_button, FALSE, FALSE, 3);
	gtk_widget_show(sharpen_button);

	/* Tlacitko pro Gaussian Blur. */
	GtkWidget *gaussian_blur_button = gtk_button_new_with_label("Gaussian Blur");
	gtk_box_pack_start(GTK_BOX(presets_box), gaussian_blur_button, FALSE, FALSE, 3);
	gtk_widget_show(gaussian_blur_button);

	/* Tlacitko pro Edge Detection 1. */
	GtkWidget *edge_detection_1_button = gtk_button_new_with_label("Edge Detection 1");
	gtk_box_pack_start(GTK_BOX(presets_box), edge_detection_1_button, FALSE, FALSE, 3);
	gtk_widget_show(edge_detection_1_button);

	/* Tlacitko pro Emboss. */
	GtkWidget *emboss_button = gtk_button_new_with_label("Emboss");
	gtk_box_pack_start(GTK_BOX(presets_box), emboss_button, FALSE, FALSE, 3);
	gtk_widget_show(emboss_button);

	// ========================================

	/* Pole ukazatelu na objekty GUI. */
	GtkObject *gui_objects[] =
	{
		GTK_OBJECT(spinbutton_0_0), // 0
		GTK_OBJECT(spinbutton_0_1), // 1
		GTK_OBJECT(spinbutton_0_2), // 2
		GTK_OBJECT(spinbutton_1_0), // 3
		GTK_OBJECT(spinbutton_1_1), // 4
		GTK_OBJECT(spinbutton_1_2), // 5
		GTK_OBJECT(spinbutton_2_0), // 6
		GTK_OBJECT(spinbutton_2_1), // 7
		GTK_OBJECT(spinbutton_2_2), // 8
		GTK_OBJECT(dialog), // 9
		GTK_OBJECT(preview), // 10
		GTK_OBJECT(filter_combobox), // 11
		GTK_OBJECT(radius_box), // 12
		GTK_OBJECT(radius_scale_core), // 13
		GTK_OBJECT(matrix), // 14
		GTK_OBJECT(presets) // 15
	};
	
	// ========================================

	/* Posluchace udalosti. */
	g_signal_connect_swapped (preview,                  "invalidated",   G_CALLBACK(convolution_2),     drawable);
	g_signal_connect(G_OBJECT(filter_combobox),         "changed",       G_CALLBACK(radius_changes),    gui_objects);
	g_signal_connect(G_OBJECT(filter_combobox),         "changed",       G_CALLBACK(customize_changes), gui_objects);
	g_signal_connect(G_OBJECT(filter_combobox),         "changed",       G_CALLBACK(preview_changes),   gui_objects);
	g_signal_connect(G_OBJECT(radius_scale_core),       "value-changed", G_CALLBACK(preview_changes),   gui_objects);
	g_signal_connect(G_OBJECT(sharpen_button),          "clicked",       G_CALLBACK(s_changes),         gui_objects);
	g_signal_connect(G_OBJECT(gaussian_blur_button),    "clicked",       G_CALLBACK(gb_changes),        gui_objects);
	g_signal_connect(G_OBJECT(edge_detection_1_button), "clicked",       G_CALLBACK(ed_1_changes),      gui_objects);
	g_signal_connect(G_OBJECT(emboss_button),           "clicked",       G_CALLBACK(e_changes),         gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_0_0),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_0_1),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_0_2),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_1_0),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_1_1),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_1_2),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_2_0),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_2_1),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);
	g_signal_connect(G_OBJECT(spinbutton_2_2),          "value-changed", G_CALLBACK(matrix_changes),    gui_objects);

	// ========================================

	/* Resetovani konvolucni matice pro pripad opakovaneho pouziti filtru a operace zpet. */
	static gui_data tmp = {1, {-1, -1, -1, -1, 9, -1, -1, -1, -1}};
	gui_values = tmp;

	/* Zobrazi hlavni dialog. */
	gtk_widget_show(dialog);

	/* Ceka na odpoved z hlavniho dialogu. */
	gboolean run = (gimp_dialog_run(GIMP_DIALOG(dialog)) == GTK_RESPONSE_OK);
	
	/* Odstrani hlavni dialog. */
	gtk_widget_destroy(dialog);

	return run;
} // SHOW_DIALOG
