extends Node2D


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	var col = import_CSV("res://Book1.csv")
	print(col[2].max())
	print(col[0].size())
	
	for i in range(1, col[0].size()):
		$Line2D.add_point(Vector2(remap(col[2][i], col[2].min(), col[2].max(), 0, 500), remap(col[1][i], col[1].min(), col[1].max(), 0, 500)),i)
		#print(remap(col[2][i], col[2].min(), col[2].max(), 0, 500))
		

	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func import_CSV(file_path: String):
	var col = []
	var file = FileAccess.open(file_path, FileAccess.READ)
	if file:
		var lines = file.get_as_text().strip_edges().split("\n")
		#print(lines)
		file.close()
		
		if lines.size() > 0:
			var headers = lines[0].split(",")
			for i in headers:
				col.append([])
			
			for i in range(1, lines.size()):
				var row = lines[i].split(",")
				for j in range(row.size()):
					col[j].append(float(row[j]))
	return col

func build_grad(list: Array):
	var grad
	
	return grad
