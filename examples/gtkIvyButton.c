#include <gtk/gtk.h>
#include <ivy.h>
#include <ivygtkloop.h>
#include <stdio.h>
#include <stdlib.h>

void hello( GtkWidget *widget, gpointer   data ) {
  fprintf(stderr,"%s\n",*((char**)data));
  IvySendMsg(*((char**)data));
}

void textCallback(IvyClientPtr app, void *user_data, int argc, char *argv[]){
  *((char **)user_data)=argv[0];
}

int main( int   argc, char *argv[] ) {
    GtkWidget *window;
    GtkWidget *button;
    char *bus=getenv("IVYBUS");
    char *tosend="foo";
    gtk_init (&argc, &argv);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    button = gtk_button_new_with_label ("send message");
    g_signal_connect (G_OBJECT(button),"clicked",G_CALLBACK(hello),&tosend); 
    gtk_container_add (GTK_CONTAINER(window),button);
    gtk_widget_show (button);
    gtk_widget_show (window);
    IvyInit ("IvyGtkButton", "IvyGtkButton READY",NULL,NULL,NULL,NULL);
    IvyBindMsg(textCallback,&tosend,"^Ivy Button text=(.*)");
    IvyStart (bus);
    gtk_main ();
    return 0;
}
