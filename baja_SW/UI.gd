extends Node2D
var col
var play
var time_elapsed: float
var temp
var start_pos
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	var file_path = "res://LOG7.csv"
	col = import_CSV(file_path)
	print(col[0])
	print(col[6])
	$HSlider.set_max(col[0].size()-1)
	temp = 0
	start_pos = $ColorRect.global_position.y
	
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
