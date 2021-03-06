#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <X11/Xlib.h>

void mouse_pos(Display *display, Window *root, int *x, int *y)
{
    XEvent evt;
    XQueryPointer(display, *root, &evt.xbutton.root, &evt.xbutton.subwindow,
                &evt.xbutton.x_root, &evt.xbutton.y_root,
                &evt.xbutton.x, &evt.xbutton.y, &evt.xbutton.state);

    *x = evt.xbutton.x_root;
    *y = evt.xbutton.y_root;
}

void send_mouse_coords(int sock)
{
    Display *display = XOpenDisplay(0);
    int scr = XDefaultScreen(display);
    Window root = XRootWindow(display, scr);

    XSelectInput(display, root, KeyPressMask | KeyReleaseMask);

    XEvent evt;

    bool control_mouse = false;
    bool shell = false;

    while (true)
    {
        if (shell)
        {
            printf("> ");
            char cmd[98] = { 0 };
            fgets(cmd, 97, stdin);
            cmd[strlen(cmd) - 1] = '\0';

            if (strcmp(cmd, "quit") == 0)
            {
                printf("Quitting shell\n");
                shell = false;
            }
            else
            {
                char buf[100] = { 0 };
                sprintf(buf, "4 %s", cmd);
                send(sock, buf, 100 * sizeof(char), 0);
                continue;
            }
        }

        XGrabPointer(display, root, True,
                    PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        XAllowEvents(display, AsyncPointer, CurrentTime);

        if (XPending(display))
        {
            XNextEvent(display, &evt);

            if (evt.type == KeyPress)
            {
                char *s = XKeysymToString(XKeycodeToKeysym(display, evt.xkey.keycode, 0));

                if (strcmp(s, "m") == 0)
                {
                    control_mouse = !control_mouse;

                    if (control_mouse)
                        printf("Controlling mouse\n");
                    else
                        printf("Released mouse\n");
                }
                else if (strcmp(s, "y") == 0)
                {
                    send(sock, "3", 2 * sizeof(char), 0);
                    continue;
                }
                else if (strcmp(s, "s") == 0)
                {
                    shell = true;
                    printf("Opened remote shell\n");
                    continue;
                }

                if (evt.xkey.keycode == 0x09)
                {
                    printf("Quitting\n");
                    send(sock, "1", 2 * sizeof(char), 0);
                    return;
                }
            }

            if (evt.type == ButtonPress || evt.type == ButtonRelease)
            {
                char s[100] = { 0 };
                sprintf(s, "2 %d %d", evt.xbutton.button, evt.type == ButtonPress ? True : False);

                send(sock, s, 100 * sizeof(char), 0);
                continue;
            }
        }

        if (control_mouse)
        {
            int x, y;
            mouse_pos(display, &root, &x, &y);
            char s[100] = { 0 };
            sprintf(s, "0 %d %d", x, y);

            send(sock, s, 100 * sizeof(char), 0);
        }

        usleep(1e4);
    }

    XCloseDisplay(display);
}

int main(int argc, char **argv)
{
    int server_fd;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        fprintf(stderr, "Failed to create socket fd\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        fprintf(stderr, "Error in setsockopt\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0)
    {
        fprintf(stderr, "Bind failed\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        fprintf(stderr, "Failed to listen\n");
        exit(EXIT_FAILURE);
    }

    printf("Server: Listening for connections\n");

    int addrlen = sizeof(address);
    int new_socket;

    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
    {
        fprintf(stderr, "Error when accepting connection\n");
        exit(EXIT_FAILURE);
    }

    printf("Server: accepted connection\n");

    send_mouse_coords(new_socket);

    return 0;
}

