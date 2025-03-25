extends Node2D
@onready var chart: Chart = $Chart
var col
var play
var time_elapsed: float
var temp
var start_pos
var f1: Function
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	var file_path = "res://LOG7.csv"
	col = import_CSV(file_path)
	print(col[0])
	print(col[6])
	$HSlider.set_max(col[0].size()-1)
	temp = 0
	start_pos = $ColorRect.global_position.y
	var cp: ChartProperties = ChartProperties.new()
	cp.colors.frame = Color("#161a1d")
	cp.colors.background = Color.TRANSPARENT
	cp.colors.grid = Color("#283442")
	cp.colors.ticks = Color("#283442")
	cp.colors.text = Color.WHITE_SMOKE
	cp.draw_bounding_box = false
	cp.title = "Length v Time"
	cp.x_label = "Time"
	cp.y_label = "Sensor values"
	cp.x_scale = 500
	cp.y_scale = 200
	cp.interactive = true
	f1 = Function.new(col[6], col[0], "val",{ 
			color = Color("#36a2eb"), 		# The color associated to this function
			marker = Function.Marker.CIRCLE, 	# The marker that will be displayed for each drawn point (x,y)
											# since it is `NONE`, no marker will be shown.
			type = Function.Type.LINE, 		# This defines what kind of plotting will be used, 
											# in this case it will be a Linear Chart.
			interpolation = Function.Interpolation.STAIR	# Interpolation mode, only used for 
															# Line Charts and Area Charts.
		})
		
	chart.plot([f1], cp)
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	
	time_elapsed += delta
	
	if play:
		if time_elapsed >= .01:
			temp += 1
			$HSlider.value = temp
			time_elapsed = 0
			if temp > $HSlider.max_value - 1:
				play = false
				temp = 0
		
			
	pass

func import_CSV(file_path: String):
	var col = []
	var file = FileAccess.open(file_path, FileAccess.READ)
	if file:
		var lines = file.get_as_text().strip_edges().split("\n")
		print(lines)
		file.close()
		
		if lines.size() > 0:
			var headers = lines[0].split(",")
			for i in headers:
				col.append([])
			
			for i in range(1, lines.size()):
				var row = lines[i].split(",")
				for j in range(row.size()):
					col[j].append(int(row[j]))
	return col


func _on_h_slider_value_changed(value: float) -> void:
	$RichTextLabel.text = str(col[0][value])
	$ColorRect.global_position.y = start_pos+col[0][value]
	#print(value)


func _on_button_pressed() -> void:
	if !play:
		play = true
	pass # Replace with function body.


func _on_stop_pressed() -> void:
	play = false
	pass # Replace with function body.
