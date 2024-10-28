# Chrome as a Local Web Server
The Chrome Developer Tools can be used as a versatile tool for web development. They allow overriding resources used by web pages and replacing them with the own local resources. Any change made to local web page source file is reflected live in the web browser.

The Chrome local override feature can be used to create a web server to serve static web pages. And name can be used as the domain name of the web server. Even if the domain name already exists, Chrome will serve pages from the local web server if the requested page or resource exists.

# Developing the device's Web interface
1. Load the Chrome and open Developer Tools window by pressing F12.
2. Click "Sources" tab and then select "Overrides" sub-tab on the left navigation panel.
3. Click "+ Select folder for overrides".
4. In the folder selection dialog, select [fsdata](../main/http/server/fsdata) as the root of all overrides.
5. When Chrome asks to confirm access, click "Allow" to let Chrome access the folder contents.
6. The folder and contents should appear in the navigation panel.
7. In the browser window enter "fs/" to the address string.
8. The device's web page should appear.
9. Modify the web page source in the "Sources" tab.
10. Save the modifications.
11. Click "Refresh" button in the browser.
12. The modified device's web page should appear.