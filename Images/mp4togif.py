from moviepy.editor import VideoFileClip

clip = (VideoFileClip("C:/Users/User16/Documents/RTOS_UNAL/Images/ESP.mp4")
 ) # Resize video to make it fit into GIF

clip.write_gif("ESP_GIF.gif")
